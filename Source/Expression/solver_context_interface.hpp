#ifndef SOLVER_CONTEXT_RESPONSES_HPP
#define SOLVER_CONTEXT_RESPONSES_HPP

#include "expression_tree.hpp"
#include "evaluation_context.hpp"

/*
  If f : Nat -> Nat is defined by cases then...
    f var_1 - open; zero or (succ n) might match var_1
    f arg(0) - head_closed; known not to match

  If g : Nat -> Nat is defined by g x = <...>, then...
    g - lambda_like

  General (assuming expression is simplified):
    An expression is *lambda_like* if applying more arguments would lead to new simplifications.
      -Must be a function type.
    An expression is *head_closed* if no further simplifications could take place on the spine,
      even after replacing open expression and applying more arguments.
    Other expressions are open.

*/

namespace expression::solver {
  enum class SimplificationState {
    head_closed, //no further simplifications could occur on spine of expression, even if more arguments were applied
    lambda_like, //applying more arguments would lead to more simplification
    open //
  };
  struct Simplification {
    SimplificationState state;
    tree::Expression expression;
  };
}

#endif
