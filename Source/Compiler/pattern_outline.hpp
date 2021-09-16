#ifndef PATTERN_OUTLINE_HPP
#define PATTERN_OUTLINE_HPP

#include "pattern_outline_impl.hpp"
#include "../Expression/evaluation_context.hpp"
#include <optional>
#include <unordered_set>
#include <unordered_map>

namespace compiler::pattern {
  struct Constraint {
    std::uint64_t capture_point;
    expression::tree::Expression equivalent_expression; //uses args for capture points
  };
  struct ConstrainedPattern {
    Pattern pattern;
    std::vector<Constraint> constraints;
    std::unordered_map<std::uint64_t, std::uint64_t> args_to_captures;
  };
  std::optional<ConstrainedPattern> from_expression(expression::tree::Expression const& expr, expression::Context const& expression_context, std::unordered_set<std::uint64_t> const& indeterminates);
}

#endif
