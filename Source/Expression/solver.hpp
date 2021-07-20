#ifndef EXPRESSION_SOLVER_HPP
#define EXPRESSION_SOLVER_HPP

#include "evaluation_context.hpp"

namespace expression {
  template<class T>
  concept SolverContext = requires(T context) {
    context;
  };
  struct SolvePart {
    tree::Tree source_type;
    tree::Tree source;
    tree::Tree target_type;
    std::uint64_t cast_result;
  };

}

#endif
