#include "runtime.hpp"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <unordered_map>
#include <iostream>

using namespace llvm;

namespace runtime {
  struct environment::_impl {
    std::unique_ptr<orc::LLJIT> jit;
    static _impl create() {
      static bool needs_init = true;
      if(needs_init) {
        needs_init = false;
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
      }
      auto jit = cantFail(orc::LLJITBuilder().create());
      return {.jit = std::move(jit)};
    }
  };
  environment::environment():data(new _impl(_impl::create())){}
  environment::~environment() = default;
  namespace {
    std::size_t get_block_local_size(block const& b) {
      std::size_t ret = b.n_args;
      for(auto const& i : b.instructions) {
        ret += !std::holds_alternative<instructions::body::store>(i);
      }
      return ret;
    }
    struct block_associated_data {
      block const* me;
      block const* parent;
      BasicBlock* bb;
      IRBuilder<> builder;
      std::size_t incoming_branches = 0;
      std::vector<PHINode*> args;
      block_associated_data(Function* f, LLVMContext& context, block const* me, block const* parent):me(me),parent(parent),bb(BasicBlock::Create(context, "", f)),builder(bb){}
    };
    struct function_data {
      Function* func;
      BasicBlock* entry;
      std::unordered_map<block const*, block_associated_data> block_data;
      block const* get_block_at_relative_index(block const* base, std::size_t index) const {
        if(index < base->children.size()) {
          return &base->children[index];
        } else {
          block_associated_data const& d = block_data.at(base);
          index -= base->children.size();
          if(d.parent) {
            return get_block_at_relative_index(d.parent, index);
          } else {
            assert(index == 0);
            return base;
          }
        }
      }
    };
    void append_function_data(LLVMContext& context, function_data& f, block const* parent, block const* cur) {
      f.block_data.insert(std::make_pair(cur, block_associated_data{f.func, context, cur, parent}));
      for(block const& child : cur->children) {
        append_function_data(context, f, cur, &child);
      }
    }
    void count_links(function_data& f, block const* cur) {
      auto const& my_data = f.block_data.at(cur);
      if(std::holds_alternative<instructions::terminator::jump>(cur->end)) {
        block const* target = f.get_block_at_relative_index(cur, std::get<instructions::terminator::jump>(cur->end).spec.target_index);
        f.block_data.at(target).incoming_branches++;
      } else if(std::holds_alternative<instructions::terminator::br>(cur->end)) {
        auto const& br = std::get<instructions::terminator::br>(cur->end);
        block const* target_t = f.get_block_at_relative_index(cur, br.if_true.target_index);
        block const* target_f = f.get_block_at_relative_index(cur, br.if_false.target_index);
        f.block_data.at(target_t).incoming_branches++;
        f.block_data.at(target_f).incoming_branches++;
      }
      for(block const& child : cur->children) {
        count_links(f, &child);
      }
    }
    void build_phi_nodes(LLVMContext& context, function_data& f, block const* cur) {
      auto& my_data = f.block_data.at(cur);
      for(std::size_t i = 0; i < cur->n_args; ++i) {
        auto node = my_data.builder.CreatePHI(Type::getInt64Ty(context), my_data.incoming_branches);
        my_data.args.push_back(node);
      }
      for(block const& child : cur->children) {
        build_phi_nodes(context, f, &child);
      }
    }
    function_data create_function_data(Module& mod, std::string const& id, LLVMContext& context, block const& b) {
      std::vector<Type*> args(b.n_args, Type::getInt64Ty(context));
      function_data f{
        .func = Function::Create(
          FunctionType::get(Type::getInt64Ty(context), args, false),
          Function::ExternalLinkage, id, mod)
      };
      f.entry = BasicBlock::Create(context, "entry", f.func);
      append_function_data(context, f, nullptr, &b);
      f.block_data.at(&b).incoming_branches++; //from entry
      count_links(f, &b);
      build_phi_nodes(context, f, &b);
      return f;
    }
    struct local_data {
      std::vector<llvm::Value*> local_values;
      void enter_block(block_associated_data const& b) {
        for(auto const& arg : b.args) {
          local_values.push_back(arg);
        }
      }
      void exit_block(block const* b) {
        local_values.resize(local_values.size() - get_block_local_size(*b));
      }
    };
    struct block_body_data {
      LLVMContext& context;
      IRBuilder<>& builder;
      local_data& data;
      Value* get_local(local_value v){ return data.local_values[v.index]; }
    };
    struct body_emit_visitor: block_body_data {
      void operator()(instructions::body::load const& i) {
        data.local_values.push_back(builder.CreateLoad(builder.CreateBitCast(get_local(i.where), Type::getInt64PtrTy(context))));
      }
      void operator()(instructions::body::store const& i) {
        builder.CreateStore(get_local(i.value.index), builder.CreateBitCast(get_local(i.target.index), Type::getInt64PtrTy(context)));
      }
      void operator()(instructions::body::constant const& i){
        data.local_values.push_back(ConstantInt::get(context, APInt(64, i.value)));
      }
      void operator()(instructions::body::add const& i){
        data.local_values.push_back(builder.CreateAdd(get_local(i.lhs), get_local(i.rhs)));
      }
      void operator()(instructions::body::sub const& i){
        data.local_values.push_back(builder.CreateSub(get_local(i.lhs), get_local(i.rhs)));
      }
      void operator()(instructions::body::cmp const& i){
        data.local_values.push_back(builder.CreateZExt(builder.CreateICmpEQ(get_local(i.lhs), get_local(i.rhs)), Type::getInt64Ty(context)));
      }
    };
    struct body_terminator_visitor: block_body_data {
      function_data const& f;
      block const* me;
      BasicBlock* fill_phi_nodes(jump_spec const& j) {
        auto const& my_data = f.block_data.at(me);
        auto target_block = f.get_block_at_relative_index(me, j.target_index);
        auto const& target_data = f.block_data.at(target_block);
        assert(j.args.size() == target_block->n_args);
        for(std::size_t i = 0; i < target_block->n_args; ++i) {
          target_data.args[i]->addIncoming(get_local(j.args[i]), my_data.bb);
        }
        return target_data.bb;
      }
      void operator()(instructions::terminator::ret const& i) {
        builder.CreateRet(get_local(i.value));
      }
      void operator()(instructions::terminator::jump const& i) {
        builder.CreateBr(fill_phi_nodes(i.spec));
      }
      void operator()(instructions::terminator::br const& i) {
        assert(i.if_true.target_index != i.if_false.target_index);
        builder.CreateCondBr(
          builder.CreateICmpNE(ConstantInt::get(context, APInt(64, 0)), get_local(i.condition)),
          fill_phi_nodes(i.if_true),
          fill_phi_nodes(i.if_false)
        );
      }
    };
    void create_entry(LLVMContext& context, function_data& f, block const* top) {
      auto& top_data = f.block_data.at(top);
      auto arg_it = f.func->arg_begin();
      for(std::size_t i = 0; i < top->n_args; ++i) {
        top_data.args[i]->addIncoming(&*arg_it, f.entry);
        ++arg_it;
      }
      IRBuilder<> builder(f.entry);
      builder.CreateBr(top_data.bb);
    }
    void generate_block(LLVMContext& context, function_data& f, local_data& d, block const* b) {
      auto& my_data = f.block_data.at(b);
      d.enter_block(my_data);
      body_emit_visitor body{ context, my_data.builder, d };
      for(auto const& i : b->instructions) std::visit(body, i);
      body_terminator_visitor terminator{ context, my_data.builder, d, f, b };
      std::visit(terminator, b->end);
      for(auto const& c : b->children) {
        generate_block(context, f, d, &c);
      }
      d.exit_block(b);
    }
    void optimize_function(Module* mod, Function* func) {
      auto fpm = std::make_unique<legacy::FunctionPassManager>(mod);
      fpm->add(createInstructionCombiningPass());
      fpm->add(createReassociatePass());
      fpm->add(createGVNPass());
      fpm->add(createCFGSimplificationPass());
      fpm->doInitialization();
      fpm->run(*func);
    }
  }
  environment::fn_type environment::compile(block const& b) {
    auto context = std::make_unique<LLVMContext>();
    auto module = std::make_unique<Module>("", *context);
    auto f = create_function_data(*module, "f", *context, b);
    local_data d;
    create_entry(*context, f, &b);
    generate_block(*context, f, d, &b);
    //verifyFunction(*f.func);
    std::cout << "Function (raw): ";
    f.func->print(llvm::outs());
    std::cout << "\n";
    optimize_function(module.get(), f.func);
    std::cout << "Function (optimized): ";
    f.func->print(llvm::outs());
    std::cout << "\n";
    orc::ThreadSafeModule mod(std::move(module), std::move(context));
    cantFail(data->jit->addIRModule(std::move(mod)));
    auto address = cantFail(data->jit->lookup("f"));
    auto func = (std::size_t(*)(std::size_t))address.getAddress();
    return func;
  }
}
