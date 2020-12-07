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
#include "llvm/Transforms/Utils.h"
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
      std::size_t ret = b.args.size();
      for(auto const& i : b.instructions) {
        ret += !std::holds_alternative<instructions::body::store>(i);
      }
      return ret;
    }
    struct module_data;
    struct function_data;
    struct block_data;
    struct block_data {
      function_data& func;
      block const* parent;
      block const* me;
      BasicBlock* bb;
      IRBuilder<> builder;
      std::size_t incoming_branches;
      std::vector<PHINode*> args;
      block_data(function_data&, block const* parent, block const* me);
    };
    struct function_data {
      module_data& mod;
      Function* func;
      BasicBlock* entry;
      IRBuilder<> entry_builder;
      std::unordered_map<block const*, block_data> b_data;
      block const* get_block_at_relative_index(block const* base, std::size_t index) const;
      Value* create_alloca(type::any const&);
      function_data(module_data& mod, std::string const& name, block const* top);
    };
    struct module_data {
      LLVMContext& context;
      Module& mod;
      std::unordered_map<type::any, Type*> types;
      Type* get_type(type::any const& t);
    };
    block_data::block_data(function_data& func, block const* parent, block const* me):
      func(func),
      parent(parent),
      me(me),
      bb(BasicBlock::Create(func.mod.context, "", func.func)),
      builder(bb),
      incoming_branches(0)
    {}
    block const* function_data::get_block_at_relative_index(block const* base, std::size_t index) const {
      if(index < base->children.size()) {
        return &base->children[index];
      } else {
        auto const& d = b_data.at(base);
        index -= base->children.size();
        if(d.parent) {
          return get_block_at_relative_index(d.parent, index);
        } else {
          assert(index == 0);
          return base;
        }
      }
    }
    Value* function_data::create_alloca(type::any const& t) {
      return entry_builder.CreateAlloca(mod.get_type(t), 0, nullptr, "mutvar");
    }
    namespace {
      Type* form_union(module_data& mod, std::vector<Type*> const& ts) {
        //todo: deal with alignments properly
        DataLayout const& layout = mod.mod.getDataLayout();
        std::size_t max_size = 0;
        //std::size_t max_align = 1;
        for(auto t : ts) {
          //std::size_t t_align = layout.getABITypeAlign(t).value();
          std::size_t t_size = layout.getTypeStoreSize(t);
          if(t_size > max_size) max_size = t_size;
        }
        std::size_t u64s = (max_size + 7) / 8;
        return ArrayType::get(Type::getInt64Ty(mod.context), u64s);
      }
      Type* form_struct(module_data& mod, std::vector<Type*> const& ts) {
        return StructType::get(mod.context, ts);
      }
      Type* form_pointer(module_data& mod, Type* t) {
        return PointerType::getUnqual(t);
      }
      Type* form_primitive(module_data& mod, type::primitive p) {
        switch(p) {
          case type::u1: return Type::getInt1Ty(mod.context);
          case type::u64: return Type::getInt64Ty(mod.context);
        }
        std::terminate();
      }
      Type* form_function(module_data& mod, Type* ret, std::vector<Type*> const& args) {
        return FunctionType::get(ret, args, false);
      }
    }
    Type* module_data::get_type(type::any const& t) {
      auto existing = types.find(t);
      if(existing != types.end()) return existing->second;
      auto type = std::visit([&](auto const& t){
        using T = std::remove_cv_t<std::remove_reference_t<decltype(t)> >;
        if constexpr(std::is_same_v<T, type::primitive>) {
          return form_primitive(*this, t);
        } else if constexpr(std::is_same_v<T, type::pointer>) {
          return form_pointer(*this, get_type(t.element));
        } else if constexpr(std::is_same_v<T, type::union_>) {
          std::vector<Type*> variants;
          for(auto const& t : t.variants) { variants.push_back(get_type(t)); }
          return form_union(*this, variants);
        } else if constexpr(std::is_same_v<T, type::structure>) {
          std::vector<Type*> members;
          for(auto const& t : t.members) { members.push_back(get_type(t)); }
          return form_struct(*this, members);
        } else if constexpr(std::is_same_v<T, type::function>) {
          Type* ret = get_type(t.ret);
          std::vector<Type*> args;
          for(auto const& t : t.args) { args.push_back(get_type(t)); }
          return form_function(*this, ret, args);
        }
      }, *t);
      types.insert(std::make_pair(t, type));
      return type;
    }
    void append_function_data(function_data& f, block const* parent, block const* cur) {
      f.b_data.insert(std::make_pair(cur, block_data{f, parent, cur}));
      for(block const& child : cur->children) {
        append_function_data(f, cur, &child);
      }
    }
    void count_links(function_data& f, block const* cur) {
      auto const& my_data = f.b_data.at(cur);
      if(std::holds_alternative<instructions::terminator::jump>(cur->end)) {
        block const* target = f.get_block_at_relative_index(cur, std::get<instructions::terminator::jump>(cur->end).spec.target_index);
        f.b_data.at(target).incoming_branches++;
      } else if(std::holds_alternative<instructions::terminator::br>(cur->end)) {
        auto const& br = std::get<instructions::terminator::br>(cur->end);
        block const* target_t = f.get_block_at_relative_index(cur, br.if_true.target_index);
        block const* target_f = f.get_block_at_relative_index(cur, br.if_false.target_index);
        f.b_data.at(target_t).incoming_branches++;
        f.b_data.at(target_f).incoming_branches++;
      }
      for(block const& child : cur->children) {
        count_links(f, &child);
      }
    }
    void build_phi_nodes(function_data& f, block const* cur) {
      auto& my_data = f.b_data.at(cur);
      for(auto const& arg : cur->args) {
        auto node = my_data.builder.CreatePHI(f.mod.get_type(arg), my_data.incoming_branches, "blockarg");
        my_data.args.push_back(node);
      }
      for(block const& child : cur->children) {
        build_phi_nodes(f, &child);
      }
    }
    Function* make_function_for(module_data& mod, std::string const& name, block const* top){
      std::vector<Type*> args;
      for(auto const& arg : top->args){ args.push_back(mod.get_type(arg)); }
      return Function::Create(
        FunctionType::get(Type::getInt64Ty(mod.context), args, false),
        Function::ExternalLinkage, name, mod.mod
      );
    }
    function_data::function_data(module_data& mod, std::string const& name, block const* top):
      mod(mod),
      func(make_function_for(mod, name, top)),
      entry(BasicBlock::Create(mod.context, "entry", func)),
      entry_builder(entry)
    {
       append_function_data(*this, nullptr, top);
       b_data.at(top).incoming_branches++; //from entry
       count_links(*this, top);
       build_phi_nodes(*this, top);
    }
    struct local_data {
      struct value_info {
        llvm::Value* v;
        type::any t;
        value_info(llvm::Value* v = nullptr, type::any t = nullptr):v(v),t(t){}
      };
      std::vector<value_info> local_values;
      void enter_block(block_data const& b) {
        for(auto const& arg : b.args) {
          local_values.push_back({arg, type::make(type::u64)});
        }
      }
      void exit_block(block const* b) {
        local_values.resize(local_values.size() - get_block_local_size(*b));
      }
    };
    struct body_emit_visitor {
      block_data& block;
      local_data& data;
      Value* get_local(local_value v){ return data.local_values[v.index].v; }
      void operator()(instructions::body::load const& i) {
        data.local_values.push_back(block.builder.CreateLoad(get_local(i.where), "loadtmp"));
      }
      void operator()(instructions::body::store const& i) {
        block.builder.CreateStore(get_local(i.value.index), get_local(i.target.index));
      }
      void operator()(instructions::body::constant const& i){
        data.local_values.push_back(ConstantInt::get(block.func.mod.context, APInt(64, i.value)));
      }
      void operator()(instructions::body::constant_fn const& i) {
        auto f_ptr_t = type::make_pointer(make_function(i.ret, i.args));
        data.local_values.push_back({block.builder.CreateBitCast(ConstantInt::get(block.func.mod.context, APInt(64, (std::size_t)i.function)), block.func.mod.get_type(f_ptr_t), "importedfunc"), f_ptr_t});
      }
      void operator()(instructions::body::local_mutable const& i){
        data.local_values.push_back({block.func.create_alloca(i.t), type::make_pointer(i.t)});
      }
      void operator()(instructions::body::add const& i){
        data.local_values.push_back(block.builder.CreateAdd(get_local(i.lhs), get_local(i.rhs), "addtmp"));
      }
      void operator()(instructions::body::sub const& i){
        data.local_values.push_back(block.builder.CreateSub(get_local(i.lhs), get_local(i.rhs), "subtmp"));
      }
      void operator()(instructions::body::cmp const& i){
        data.local_values.push_back(block.builder.CreateICmpEQ(get_local(i.lhs), get_local(i.rhs), "cmptmp"));
      }
      void operator()(instructions::body::get_element_ptr const& i) {
        auto const& v_info = data.local_values[i.value.index];
        assert(v_info.t);
        assert(std::holds_alternative<type::pointer>(*v_info.t));
        auto const& ag_type = *std::get<type::pointer>(*v_info.t).element;
        if(std::holds_alternative<type::union_>(ag_type)) {
          auto const& u = std::get<type::union_>(ag_type);
          assert(i.index < u.variants.size());
          auto result_t = type::make_pointer(u.variants[i.index]);
          data.local_values.push_back({block.builder.CreateBitCast(v_info.v, block.func.mod.get_type(result_t), "unionaccess"), result_t});
        } else if(std::holds_alternative<type::structure>(ag_type)) {
          auto const& s = std::get<type::structure>(ag_type);
          assert(i.index < s.members.size());
          std::vector<Value*> indices;
          indices.push_back(ConstantInt::get(block.func.mod.context, APInt(64, 0)));
          indices.push_back(ConstantInt::get(block.func.mod.context, APInt(32, i.index)));
          auto result_t = type::make_pointer(s.members[i.index]);
          data.local_values.push_back({block.builder.CreateGEP(v_info.v, indices, "structaccess"), result_t});
        } else {
          std::terminate();
        }
      }
      void operator()(instructions::body::get_index_ptr const& i) {
        auto const& v_info = data.local_values[i.value.index];
        std::vector<Value*> indices;
        indices.push_back(get_local(i.index));
        data.local_values.push_back({block.builder.CreateGEP(v_info.v, indices, "arrayaccess"), v_info.t});
      }
      void operator()(instructions::body::call const& i) {
        auto const& f_info = data.local_values[i.callee.index];
        assert(f_info.t);
        assert(std::holds_alternative<type::pointer>(*f_info.t));
        auto base_type = std::get<type::pointer>(*f_info.t).element;
        assert(std::holds_alternative<type::function>(*base_type));
        auto ftype = dyn_cast<FunctionType>(block.func.mod.get_type(base_type));
        assert(ftype);
        std::vector<Value*> args;
        for(auto arg : i.args) { args.push_back(get_local(arg)); }
        data.local_values.push_back({block.builder.CreateCall(ftype, f_info.v, args, "calltmp")});
      }
    };
    struct body_terminator_visitor {
      block_data& block;
      local_data& data;
      Value* get_local(local_value v){ return data.local_values[v.index].v; }
      BasicBlock* fill_phi_nodes(jump_spec const& j) {
        auto const& my_data = block.func.b_data.at(block.me);
        auto target_block = block.func.get_block_at_relative_index(block.me, j.target_index);
        auto const& target_data = block.func.b_data.at(target_block);
        assert(j.args.size() == target_block->args.size());
        for(std::size_t i = 0; i < target_block->args.size(); ++i) {
          target_data.args[i]->addIncoming(get_local(j.args[i]), my_data.bb);
        }
        return target_data.bb;
      }
      void operator()(instructions::terminator::ret const& i) {
        block.builder.CreateRet(get_local(i.value));
      }
      void operator()(instructions::terminator::jump const& i) {
        block.builder.CreateBr(fill_phi_nodes(i.spec));
      }
      void operator()(instructions::terminator::br const& i) {
        assert(i.if_true.target_index != i.if_false.target_index);
        block.builder.CreateCondBr(
          block.builder.CreateICmpNE(ConstantInt::get(block.func.mod.context, APInt(64, 0)), get_local(i.condition)),
          fill_phi_nodes(i.if_true),
          fill_phi_nodes(i.if_false)
        );
      }
    };
    void finish_entry(function_data& f, block const* top) {
      auto& top_data = f.b_data.at(top);
      auto arg_it = f.func->arg_begin();
      for(std::size_t i = 0; i < top->args.size(); ++i) {
        top_data.args[i]->addIncoming(&*arg_it, f.entry);
        ++arg_it;
      }
      f.entry_builder.CreateBr(top_data.bb);
    }
    void generate_block(function_data& f, local_data& d, block const* b) {
      auto& my_data = f.b_data.at(b);
      d.enter_block(my_data);
      body_emit_visitor body{ my_data, d };
      for(auto const& i : b->instructions) std::visit(body, i);
      body_terminator_visitor terminator{ my_data, d };
      std::visit(terminator, b->end);
      for(auto const& c : b->children) {
        generate_block(f, d, &c);
      }
      d.exit_block(b);
    }
    void optimize_function(Module* mod, Function* func) {
      auto fpm = std::make_unique<legacy::FunctionPassManager>(mod);
      fpm->add(createPromoteMemoryToRegisterPass());
      fpm->add(createInstructionCombiningPass());
      fpm->add(createReassociatePass());
      fpm->add(createGVNPass());
      fpm->add(createCFGSimplificationPass());
      fpm->doInitialization();
      fpm->run(*func);
    }
  }
  environment::fn_type environment::compile(block const& b, std::string const& name) {
    auto context = std::make_unique<LLVMContext>();
    auto module = std::make_unique<Module>("", *context);
    module_data mod{ *context, *module };
    auto f = function_data{mod, name, &b};
    local_data d;
    generate_block(f, d, &b);
    finish_entry(f, &b);
    llvm::outs() << "Function (raw): ";
    f.func->print(llvm::outs());
    llvm::outs() << "\n";
    verifyFunction(*f.func);
    optimize_function(module.get(), f.func);
    llvm::outs() << "Function (optimized): ";
    f.func->print(llvm::outs());
    llvm::outs() << "\n";
    orc::ThreadSafeModule tmod(std::move(module), std::move(context));
    cantFail(data->jit->addIRModule(std::move(tmod)));
    auto address = cantFail(data->jit->lookup(name));
    auto func = (std::size_t(*)(std::size_t))address.getAddress();
    return func;
  }
}
