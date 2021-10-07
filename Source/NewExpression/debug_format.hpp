#ifndef EXPRESSION_DEBUG_FORMAT_HPP2
#define EXPRESSION_DEBUG_FORMAT_HPP2

#include "arena.hpp"
#include "../Utility/function.hpp"

namespace new_expression {
  struct RawFormat {
    Arena const& arena;
    WeakExpression expression;
  };
  RawFormat raw_format(Arena const& arena, WeakExpression expression);
  std::ostream& operator<<(std::ostream&, RawFormat const&);
}

#endif
