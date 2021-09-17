#ifndef SOLVE_ROUTINE_HPP
#define SOLVE_ROUTINE_HPP

#include "solver.hpp"
#include "../Compiler/evaluator.hpp"
#include "standard_solver_context.hpp"

namespace expression::solver {
  class Routine {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Routine(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext solver_context);
    Routine(Routine&&);
    Routine& operator=(Routine&&);
    ~Routine();
    void run();
    std::vector<std::pair<tree::Expression, tree::Expression> > get_equations();
  };
};

#endif
