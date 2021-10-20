#ifndef COMMON_ROUTINES_HPP
#define COMMON_ROUTINES_HPP

#include "interface_types.hpp"

namespace solver::routine {
  using EquationHandler = mdb::function<mdb::Future<EquationResult>(Equation)>;
  struct CastInfo {
    stack::Stack stack;
    new_expression::OwnedExpression variable;
    new_expression::OwnedExpression source_type;
    new_expression::OwnedExpression source;
    new_expression::OwnedExpression target_type;
    new_expression::Arena& arena;
    EquationHandler handle_equation;
    mdb::function<void(new_expression::Rule)> add_rule;
  };
  mdb::Future<EquationResult> cast(CastInfo);
  struct FunctionCastInfo {
    stack::Stack stack;
    new_expression::OwnedExpression function_variable;
    new_expression::OwnedExpression argument_variable;
    new_expression::OwnedExpression function_value;
    new_expression::OwnedExpression function_type;
    new_expression::OwnedExpression expected_function_type;
    new_expression::OwnedExpression argument_value;
    new_expression::OwnedExpression argument_type;
    new_expression::OwnedExpression expected_argument_type;
    new_expression::Arena& arena;
    EquationHandler handle_equation;
    mdb::function<void(new_expression::Rule)> add_rule;
  };
  struct FunctionCastResult {
    EquationResult result;
    bool lhs_was_function;
  };
  mdb::Future<FunctionCastResult> function_cast(FunctionCastInfo);
}

#endif
