#ifndef NEW_TYPED_VALUE_HPP
#define NEW_TYPED_VALUE_HPP

#include "arena.hpp"
#include "on_arena_utility.hpp"

namespace new_expression {
  struct TypedValue {
    OwnedExpression value;
    OwnedExpression type;
    static constexpr auto part_info = mdb::parts::simple<2>;
    friend inline bool operator==(TypedValue const& lhs, TypedValue const& rhs) { return lhs.value == rhs.value && lhs.type == rhs.type; }
  };
}

#endif
