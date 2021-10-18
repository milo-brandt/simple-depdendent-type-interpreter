#ifndef NEW_EXPRESSION_DEBUG_FORMAT_HPP
#define NEW_EXPRESSION_DEBUG_FORMAT_HPP

#include "../NewExpression/arena.hpp"
#include "../Utility/function.hpp"

namespace user {
  struct RawFormat {
    new_expression::Arena& arena;
    new_expression::WeakExpression expression;
    mdb::function<void(std::ostream&, new_expression::WeakExpression)> format_external;
  };
  RawFormat raw_format(new_expression::Arena& arena, new_expression::WeakExpression expression);
  RawFormat raw_format(new_expression::Arena& arena, new_expression::WeakExpression expression, mdb::function<void(std::ostream&, new_expression::WeakExpression)> format_external);
  std::ostream& operator<<(std::ostream&, RawFormat const&);
}

#endif
