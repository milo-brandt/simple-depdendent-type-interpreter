#include "evaluation_context.hpp"

namespace expression {
  Context::Context() {
    external_info.push_back({ .is_axiom = true });
    external_info.push_back({ .is_axiom = true });
    primitives.type = 0;
    primitives.arrow = 1;
  }
  tree::Tree Context::reduce(tree::Tree tree) {
  REDUCTION_START:
    for(auto const& rule : rules) {
      auto vec = find_all_matches(tree, rule.pattern);
      if(!vec.empty()) {
        replace_with_substitution_at(tree, vec[0], rule.pattern, rule.replacement);
        goto REDUCTION_START;
      }
    }
    return tree;
  }
  std::uint64_t Context::name_expression(tree::Tree term) {
    auto index = external_info.size();
    external_info.push_back({ .is_axiom = false });
    rules.push_back({
      .pattern = pattern::Fixed{index},
      .replacement = std::move(term)
    });
    return index;
  }
}
