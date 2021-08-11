#ifndef EXPRESSION_RULE_SIMPLIFICATION_HPP
#define EXPRESSION_RULE_SIMPLIFICATION_HPP

#include "evaluation_context.hpp"
#include <unordered_set>

namespace expression::rule {
  Rule simplify_rule(Rule, Context&);
  std::uint64_t get_rule_head(Rule const&);
  struct DependencyFinder {
    Context& context;
    std::unordered_set<std::uint64_t> seen;
    void examine_external(std::uint64_t);
    void examine_expression(tree::Tree const&);
  };
};

#endif
