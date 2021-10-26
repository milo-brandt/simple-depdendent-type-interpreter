#ifndef INTERFACE_TYPES_HPP
#define INTERFACE_TYPES_HPP

#include "stack.hpp"
#include "solver.hpp"
#include "../Utility/async.hpp"

namespace solver {
  struct EquationSolved {};
  struct EquationStalled {
    EquationErrorInfo error;
    static constexpr auto part_info = mdb::parts::simple<1>;
  };
  struct EquationFailed {
    mdb::Future<EquationErrorInfo> error;
    static constexpr auto part_info = mdb::parts::simple<1>;
  };
  using EquationResult = std::variant<EquationFailed, EquationStalled, EquationSolved>;

/*
  mdb::Future<EquationResult>

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
  };*/
}

#endif
