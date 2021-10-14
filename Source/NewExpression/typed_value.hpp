#ifndef NEW_TYPED_VALUE_HPP
#define NEW_TYPED_VALUE_HPP

#include "arena.hpp"
#include "on_arena_utility.hpp"

namespace new_expression {
  struct TypedValue {
    OwnedExpression value;
    OwnedExpression type;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  TypedValue copy_on_arena(Arena& arena, TypedValue&& input) = delete;
  inline TypedValue copy_on_arena(Arena& arena, TypedValue const& input) {
    return {
      .value = arena.copy(input.value),
      .type = arena.copy(input.type)
    };
  }
}

#endif
