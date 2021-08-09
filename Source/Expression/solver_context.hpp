#ifndef SOLVER_CONTEXT_HPP
#define SOLVER_CONTEXT_HPP

#include "solver.hpp"

namespace expression::solve {
  struct SimpleContext {
    Context& context;
    tree::Tree reduce(tree::Tree input);
    bool needs_more_arguments(tree::Tree const&);
    bool is_head_closed(tree::Tree const& input, std::unordered_set<std::uint64_t> const& variables);
    void add_rule(Rule rule, rule_explanation::Any);
    std::uint64_t introduce_variable(introduction_explanation::Any);
    bool term_depends_on(std::uint64_t term, std::uint64_t check);
  };
}

#endif
