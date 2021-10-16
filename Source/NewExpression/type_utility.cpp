#include "type_utility.hpp"
#include "../Utility/overloaded.hpp"
#include "arena_utility.hpp"

namespace new_expression {
  std::optional<FunctionInfo> get_function_data(Arena& arena, WeakExpression target, WeakExpression arrow) {
    auto unfolded = unfold(arena, target);
    if(unfolded.head == arrow && unfolded.args.size() == 2) {
      return FunctionInfo{
        .domain = unfolded.args[0],
        .codomain = unfolded.args[1]
      };
    } else {
      return std::nullopt;
    }
  }
  OwnedExpression get_type_of(Arena& arena, WeakExpression expr, TypeGetterContext const& context) {
    auto unfolded = unfold(arena, expr);
    auto type_so_far = context.type_of_primitive(unfolded.head);
    for(auto& arg : unfolded.args) {
      type_so_far = context.reduce_head(std::move(type_so_far));
      if(auto as_function = get_function_data(arena, type_so_far, context.arrow)) {
        auto applied_type = arena.apply(
          arena.copy(as_function->codomain),
          arena.copy(arg)
        );
        arena.drop(std::move(type_so_far));
        type_so_far = std::move(applied_type);
      } else {
        std::terminate(); //ill-typed
      }
    }
    return type_so_far;
  }
}
