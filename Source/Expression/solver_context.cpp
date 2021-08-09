#include "solver_context.hpp"
#include <iostream>

namespace expression::solve {
  tree::Tree SimpleContext::reduce(tree::Tree input) {
    return context.reduce(input);
  }
  bool SimpleContext::needs_more_arguments(tree::Tree const& input) {
    for(auto const& rule : context.rules) {
      pattern::Tree const* pat = &rule.pattern;
      while(pattern::Apply const* app = pat->get_if_apply()) {
        if(term_matches(input, app->lhs)) {
          return true;
        }
        pat = &app->lhs;
      }
    }
    return false;
  }
  bool SimpleContext::is_head_closed(tree::Tree const& input, std::unordered_set<std::uint64_t> const& variables) {
    auto unfolding = unfold_ref(input);
    if(auto* ext = unfolding.head->get_if_external()) {
      return context.external_info[ext->index].is_axiom;
    } else {
      return true; //argument head
    }
  }
  void SimpleContext::add_rule(Rule rule, rule_explanation::Any) {
    context.rules.push_back(std::move(rule));
  }
  std::uint64_t SimpleContext::introduce_variable(introduction_explanation::Any) {
    auto ret = context.external_info.size();
    context.external_info.push_back({
      .is_axiom = false
    });
    return ret;
  }
  bool SimpleContext::term_depends_on(std::uint64_t term, std::uint64_t check) {
    return term == check;
  }

}
