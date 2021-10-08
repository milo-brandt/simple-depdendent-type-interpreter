#ifndef ARENA_UTILITY_HPP
#define ARENA_UTILITY_HPP

#include "arena.hpp"
#include <vector>

namespace new_expression {
  struct Unfolding {
    WeakExpression head;
    std::vector<WeakExpression> args;
  };
  Unfolding unfold(Arena&, WeakExpression);
  struct OwnedUnfolding {
    OwnedExpression head;
    std::vector<OwnedExpression> args;
  };
  OwnedUnfolding unfold_owned(Arena&, OwnedExpression);
  OwnedExpression substitute_into(Arena&, WeakExpression target, std::span<OwnedExpression const> args);
}

#endif
