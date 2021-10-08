#include "rule.hpp"

namespace new_expression {
  void RuleCollector::register_declaration(WeakExpression expr, bool indeterminate) {
    if(p_declaration_info.contains(expr)) std::terminate();
    p_declaration_info.set(expr, DeclarationInfo{.is_indeterminate = indeterminate});
  }
  DeclarationInfo const& RuleCollector::declaration_info(WeakExpression expr) const {
    return p_declaration_info.at(expr);
  }
  RuleCollector::RuleCollector(Arena& arena):p_arena(&arena), p_declaration_info(arena) {}
  void RuleCollector::add_rule(Rule rule) {
    p_declaration_info.at(rule.pattern.head).rules.push_back({
      .pattern_body = std::move(rule.pattern.body),
      .replacement = std::move(rule.replacement)
    });
    p_arena->drop(std::move(rule.pattern.head));
    new_rule();
  }
}
