#include "standard_solver_context.hpp"
#include <cassert>

namespace expression::solver {
  namespace {
    pattern::Pattern make_pattern_for(std::uint64_t head, std::size_t arg_count) {
      pattern::Pattern ret = pattern::Fixed{head};
      for(std::size_t i = 0; i < arg_count; ++i) {
        ret = pattern::Apply{std::move(ret), pattern::Wildcard{}};
      }
      return ret;
    }
    bool is_lambda_like(tree::Expression const& expr, expression::Context const& evaluation) {
      for(auto const& rule : evaluation.rules) {
        pattern::Pattern const* segment = &rule.pattern;
        while(auto* apply = segment->get_if_apply()) {
          if(!apply->rhs.holds_wildcard()) break;
          segment = &apply->lhs;
          if(term_matches(expr, *segment)) {
            return true;
          }
        }
      }
      return false;
    }
  }
  Simplification StandardSolverContext::simplify(tree::Expression base) {
    auto simplified = evaluation.reduce(base);
    auto unfolded = unfold_ref(simplified);
    SimplificationState state = [&] {
      if(unfolded.head->holds_arg()) {
        return SimplificationState::head_closed;
      } else {
        auto const& external = unfolded.head->get_external();
        if(evaluation.external_info[external.external_index].is_axiom) {
          return SimplificationState::head_closed;
        } else {
          if(is_lambda_like(simplified, evaluation)) {
            return SimplificationState::lambda_like;
          } else {
            return SimplificationState::open;
          }
        }
      }
    }();
    return {
      .state = state,
      .expression = std::move(simplified)
    };
  }
  void StandardSolverContext::define_variable(std::uint64_t variable, std::uint64_t arg_count, tree::Expression replacement) {
    assert(indeterminates.contains(variable));
    indeterminates.erase(variable);
    evaluation.add_rule({
      .pattern = make_pattern_for(variable, arg_count),
      .replacement = std::move(replacement)
    });
  }
  std::uint64_t StandardSolverContext::introduce_variable(tree::Expression type) {
    auto ret = evaluation.create_variable({
      .is_axiom = false,
      .type = std::move(type)
    });
    indeterminates.insert(ret);
    return ret;
  }
  bool StandardSolverContext::term_depends_on(std::uint64_t term, std::uint64_t possible_dependency) {
    return term == possible_dependency;
  }
}
