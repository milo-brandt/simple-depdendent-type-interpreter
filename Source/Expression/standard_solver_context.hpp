#ifndef STANDARD_SOLVER_CONTEXT_HPP
#define STANDARD_SOLVER_CONTEXT_HPP

#include "evaluation_context.hpp"
#include "solver.hpp"
#include <unordered_set>

namespace expression::solver {
  struct StandardSolverContext {
    expression::Context& evaluation;
    std::unordered_set<std::uint64_t> indeterminates;

    Simplification simplify(tree::Expression base);
    void define_variable(std::uint64_t variable, std::uint64_t arg_count, tree::Expression replacement);
    std::uint64_t introduce_variable();
    bool term_depends_on(std::uint64_t term, std::uint64_t possible_dependency);
  };
}

#endif
