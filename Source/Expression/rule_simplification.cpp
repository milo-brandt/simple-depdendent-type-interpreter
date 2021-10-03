#include "rule_simplification.hpp"

namespace expression::rule {
  namespace {
    std::uint64_t count_wildcards(pattern::Pattern const& pattern) {
      return pattern.visit(mdb::overloaded{
        [&](pattern::Apply const& apply) {
          return count_wildcards(apply.lhs) + count_wildcards(apply.rhs);
        },
        [&](pattern::Wildcard const&) {
          return (std::uint64_t)1;
        },
        [&](pattern::Fixed const&) {
          return (std::uint64_t)0;
        }
      });
    }
    /*bool expression_uses_arg(tree::Expression const& tree, std::uint64_t arg_index) {
      return tree.visit(mdb::overloaded{
        [&](tree::Apply const& apply) {
          return expression_uses_arg(apply.lhs, arg_index) || expression_uses_arg(apply.rhs, arg_index);
        },
        [&](tree::Arg const& arg) {
          return arg.index == arg_index;
        },
        [&](tree::External const&) {
          return false;
        }
      });
    }
    static constexpr auto lambda_like = pattern::match::Apply{
      pattern::match::Any{},
      pattern::match::Wildcard{}
    };*/
    bool needs_more_arguments(tree::Expression const& input, Context& context) {
      for(auto const& rule : context.rules) {
        pattern::Pattern const* pat = &rule.pattern;
        while(pattern::Apply const* app = pat->get_if_apply()) {
          if(term_matches(input, app->lhs)) {
            return true;
          }
          pat = &app->lhs;
        }
      }
      return false;
    }
  }
  Rule simplify_rule(Rule rule, Context& context) {
  KEEP_SIMPLIFYING:
    rule.replacement = context.reduce(rule.replacement);
    if(needs_more_arguments(rule.replacement, context)) {
      auto next_arg = count_wildcards(rule.pattern);
      rule.pattern = pattern::Apply{std::move(rule.pattern), pattern::Wildcard{}};
      rule.replacement = tree::Apply{std::move(rule.replacement), tree::Arg{next_arg}};
      goto KEEP_SIMPLIFYING;
    }
    return rule;
  }
  /*std::uint64_t get_rule_head(Rule const& rule) {
    auto const* head = &rule.pattern;
    while(auto const* apply = head->get_if_apply()) {
      head = &apply->lhs;
    }
    return head->get_fixed().external_index;
  }
  void DependencyFinder::examine_external(std::uint64_t x) {
    if(seen.contains(x)) return;
    seen.insert(x);
    for(auto const& rule : context.rules) {
      if(get_rule_head(rule) == x) {
        examine_expression(rule.replacement);
      }
    }
  }
  void DependencyFinder::examine_expression(tree::Expression const& tree) {
    tree.visit(mdb::overloaded{
      [&](tree::Apply const& apply) {
        examine_expression(apply.lhs);
        examine_expression(apply.rhs);
      },
      [&](tree::External const& ext) {
        examine_external(ext.external_index);
      },
      [&](tree::Arg const&) {},
      [&](tree::Data const&) {
        //TODO
      }
    });
  }*/
}
