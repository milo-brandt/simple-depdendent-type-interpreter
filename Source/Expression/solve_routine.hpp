#ifndef SOLVE_ROUTINE_HPP
#define SOLVE_ROUTINE_HPP

#include "solver.hpp"
#include "../Compiler/evaluator.hpp"
#include "standard_solver_context.hpp"

namespace expression::solver {
  enum class SourceKind {
    cast_equation, //an equation arising from a cast
    rule_equation, //an equation arising from checking type(LHS) = type(RHS) in rule
    rule_skeleton, //an equation arising from deriving relations among capture-point rule
    rule_skeleton_verify //an equation arising from checking requested relations among capture-points
  };
  struct HungRoutineEquation {
    tree::Expression lhs;
    tree::Expression rhs;
    SourceKind source_kind;
    std::uint64_t source_index;
    bool failed;
  };
  class Routine {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Routine(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext solver_context);
    Routine(Routine&&);
    Routine& operator=(Routine&&);
    ~Routine();
    void run();
    std::vector<HungRoutineEquation> get_hung_equations();
  };
};

#endif
