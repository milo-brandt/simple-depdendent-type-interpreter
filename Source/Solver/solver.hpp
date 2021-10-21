#ifndef NEW_SOLVER_HPP
#define NEW_SOLVER_HPP

#include <memory>
#include <unordered_set>
#include "../NewExpression/evaluation.hpp"
#include "../Utility/function.hpp"
#include "../Utility/event.hpp"
#include "stack.hpp"

/*
Solving algorithm: repeatedly loop over all equations (as long as progress is being made), and apply the following steps.

Reduce both sides of the equation fully, then to see if progress can be made by
applying any of the following rules. The first rule that works should be applied
and then no further rules should be checked until the next time the equation is
handled.

  I. Definition Extraction
          VARIABLE arg(5) arg(3) arg(1) = f arg(3) (arg(1) arg(5))
      If one side is a variable with a series of distinct arguments
      applied to and the other side...
        i.  Only uses arguments present on the left.
        ii. Does not depend in any way on VARIABLE being defined. (Including through rules)
      ...then, a new rule should be created defining VARIABLE as a lambda and the
      equation should be marked as solved.

  II. Deepening
          F = G
      If one side of the equation would reduce further if more arguments were applied
      to it, then a new argument should be introduced and applied to both sides.

  III. Symmetric Explosion
          pair x y = pair z w
      If both sides are head-closed and have the same head and same number of arguments,
      create new equations expressing the equality of corresponding arguments of the
      expression and mark the current equation as solved.

      If both sides are head-closed, but have different heads or different numbers of
      arguments, mark the equation as unsolvable.

  IV. Asymmetric Explosion
          VARIABLE arg(0) arg(1) = pair (f arg(0)) (g arg(2))
      If one side is a variable with a series of distinct arguments applied and the
      other side is head-closed and the head is either an external or one of the
      arguments applied to the variable, then...
        i.   Introduce a new variable for each argument on the head-closed side.
        ii.  Define VARIABLE to be the head applied to the new introductions
             (with appropriate arguments applied)
        iii. Create a new equation expressing that these variables equal the
             corresponding argument of the head-closed side.
      Finally, mark the current equation as solved.

  V.  Argument Reduction
          VARIABLE arg(0) = OTHER_VARIABLE arg(1)
      If one side is a variable with a series of distinct arguments applied and the
      other side is independent of one or more of those variables, introduce a new
      variable. Add a rule which asserts that VARIABLE with each argument applied
      equals the introduced variable with only the arguments present on both sides
      applied and add a new equation asserting that this new variable with the common
      arguments applied equals the second side.

  VI. Judgmental Equality
          VARIABLE = VARIABLE
      If the two sides are literally equal, mark the equation as solved.
*/

/*
  Note: should also consider allowing types with a single constructor (e.g. tuples)
  to be expanded if someone tries to match against them. Need to be careful about
  types like data Bad { cons: Bad -> Bad; } and functions like f : Bad -> Nat
  f (cons x) = f x, where f var should become f (cons var') = f var' and so on...
*/

namespace solver {
  struct Equation {
    new_expression::OwnedExpression lhs;
    new_expression::OwnedExpression rhs;
    stack::Stack stack;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct EquationErrorInfo{
    Equation primary;
    bool primary_failed;
    std::vector<Equation> failures;
    std::vector<Equation> stalls;
  };
  struct IndeterminateDefinition {
    new_expression::WeakExpression head;
    std::size_t arg_count;
    new_expression::OwnedExpression replacement;
  };
  struct StackHandle {
    std::size_t index;
  };
  struct DefinableInfo {
    stack::Stack stack;
  };
  struct SolverInterface {
    new_expression::Arena& arena;
    mdb::function<bool(new_expression::WeakExpression, new_expression::WeakExpression)> term_depends_on;
    mdb::function<std::optional<DefinableInfo>(new_expression::WeakExpression)> get_definable_info;
    mdb::function<bool(new_expression::WeakExpression)> is_lambda_like;
    mdb::function<bool(new_expression::WeakExpression)> is_head_closed;
    mdb::function<void(IndeterminateDefinition)> make_definition;
  };
  class Solver {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Solver(SolverInterface interface, Equation equation);
    Solver(Solver&&);
    Solver& operator=(Solver&&);
    ~Solver();
    bool try_to_make_progress();
    void close();
    bool solved();
    bool failed();
    EquationErrorInfo get_error_info() &&;
  };
}

#endif
