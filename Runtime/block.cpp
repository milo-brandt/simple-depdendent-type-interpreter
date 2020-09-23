#include "block.hpp"
#include <cassert>
#include <cstring>

namespace runtime {

  namespace {
    std::size_t get_self_size(block const& b) {
      std::size_t ret = b.n_args;
      for(auto const& i : b.instructions) {
        ret += !std::holds_alternative<instructions::body::store>(i);
      }
      return ret;
    }
    std::size_t get_max_stack_size(block const& b) {
      std::size_t max_child = 0;
      for(auto const& c : b.children){
        std::size_t child_size = get_max_stack_size(c);
        if(child_size > max_child) max_child = child_size;
      }
      return max_child + get_self_size(b);
    }
  }
  std::size_t block::simulate(std::size_t* args) const {
    auto stack = std::make_unique<std::size_t[]>(get_max_stack_size(*this));
    memcpy(stack.get(), args, sizeof(std::size_t) * n_args);
    std::size_t stack_ptr = n_args;
    block const* next = this;
    std::vector<block const*> block_stack;
    while(true) {
      for(auto const& i : next->instructions) {
        std::visit([&](auto const& i){
          using T = std::remove_cv_t<std::remove_reference_t<decltype(i)> >;
          if constexpr(std::is_same_v<T, instructions::body::load>) {
            stack[stack_ptr++] = *(std::size_t*)stack[i.where.index];
          } else if constexpr(std::is_same_v<T, instructions::body::store>) {
            *(std::size_t*)stack[i.target.index] = stack[i.value.index];
          } else if constexpr(std::is_same_v<T, instructions::body::constant>) {
            stack[stack_ptr++] = i.value;
          } else if constexpr(std::is_same_v<T, instructions::body::add>) {
            stack[stack_ptr++] = stack[i.lhs.index] + stack[i.rhs.index];
          } else if constexpr(std::is_same_v<T, instructions::body::sub>) {
            stack[stack_ptr++] = stack[i.lhs.index] - stack[i.rhs.index];
          } else if constexpr(std::is_same_v<T, instructions::body::cmp>) {
            stack[stack_ptr++] = stack[i.lhs.index] == stack[i.rhs.index];
          }
        }, i);
      }
      if(auto* v = std::get_if<instructions::terminator::ret>(&next->end)) {
        return stack[v->value.index];
      } else {
        jump_spec jmp = [&](){
          if(std::holds_alternative<instructions::terminator::jump>(next->end)) {
            return std::get<instructions::terminator::jump>(next->end).spec;
          } else {
            auto const& br = std::get<instructions::terminator::br>(next->end);
            return stack[br.condition.index] ? br.if_true : br.if_false;
          }
        }();
        while(true) {
          if(jmp.target_index < next->children.size()) {
            block_stack.push_back(next);
            next = &next->children[jmp.target_index];
            break;
          } else {
            stack_ptr -= get_self_size(*next);
            jmp.target_index -= next->children.size();
            if(!block_stack.empty()) {
              next = block_stack.back();
              block_stack.pop_back();
            } else {
              assert(jmp.target_index == 0);
              break;
            }
          }
        }
        assert(jmp.args.size() == next->n_args);
        std::vector<std::size_t> tmp;
        tmp.reserve(next->n_args);
        for(auto const v : jmp.args) {
          tmp.push_back(stack[v.index]);
        }
        memcpy(&stack[stack_ptr], &tmp[0], sizeof(std::size_t) * next->n_args);
        stack_ptr += jmp.args.size();
      }
    }
  }
}
