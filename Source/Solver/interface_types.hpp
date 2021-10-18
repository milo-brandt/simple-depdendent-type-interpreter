#ifndef INTERFACE_TYPES_HPP
#define INTERFACE_TYPES_HPP

#include "stack.hpp"

namespace solver {
  struct Cast {
    stack::Stack stack;
    new_expression::OwnedExpression variable;
    new_expression::OwnedExpression source_type;
    new_expression::OwnedExpression source;
    new_expression::OwnedExpression target_type;
  };
  struct FunctionCast {
    stack::Stack stack;
    new_expression::OwnedExpression function_variable;
    new_expression::OwnedExpression argument_variable;
    new_expression::OwnedExpression function_value;
    new_expression::OwnedExpression function_type;
    new_expression::OwnedExpression expected_function_type;
    new_expression::OwnedExpression argument_value;
    new_expression::OwnedExpression argument_type;
    new_expression::OwnedExpression expected_argument_type;
  };
  struct Rule {
    stack::Stack stack;
    new_expression::Rule rule;
    std::vector<std::pair<new_expression::OwnedExpression, new_expression::OwnedExpression> > checks;
  };
}

#endif
