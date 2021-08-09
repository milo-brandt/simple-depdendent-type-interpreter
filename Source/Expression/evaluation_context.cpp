#include "evaluation_context.hpp"

namespace expression {
  Context::Context() {
    external_info.push_back({ .is_axiom = true });
    external_info.push_back({ .is_axiom = true });
    external_info.push_back({ .is_axiom = false });
    external_info.push_back({ .is_axiom = false });
    external_info.push_back({ .is_axiom = false });
    primitives.type = 0;
    primitives.arrow = 1;
    primitives.arrow_type_codomain = 2;
    primitives.type_constant_function = 3;
    rules.push_back({
      .pattern = pattern::Apply{
        .lhs = pattern::Fixed{primitives.type_constant_function},
        .rhs = pattern::Wildcard{}
      },
      .replacement = tree::External{primitives.type}
    });
    rules.push_back({
      .pattern = pattern::Apply{
        .lhs = pattern::Fixed{primitives.arrow_type_codomain},
        .rhs = pattern::Wildcard{}
      },
      .replacement = tree::Apply{
        .lhs = tree::Apply{
          .lhs = tree::External{primitives.arrow},
          .rhs = tree::Apply{
            .lhs = tree::Apply{
              .lhs = tree::External{primitives.arrow},
              .rhs = tree::Arg{0}
            },
            .rhs = tree::External{primitives.type_constant_function}
          }
        },
        .rhs = tree::External{primitives.type_constant_function}
      }
    });
  }
  tree::Tree Primitives::arrow_type() {
    return tree::Apply{
      .lhs = tree::Apply{
        .lhs = tree::External{arrow},
        .rhs = tree::External{type}
      },
      .rhs = tree::External{arrow_type_codomain}
    };
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
  tree::Tree Context::reduce_once_at_root(tree::Tree tree) {
    for(auto const& rule : rules) {
      if(term_matches(tree, rule.pattern)) {
        replace_with_substitution_at(tree, {}, rule.pattern, rule.replacement);
        return tree;
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
  std::uint64_t Context::add_axiom() {
    auto index = external_info.size();
    external_info.push_back({ .is_axiom = true });
    return index;
  }
  std::uint64_t Context::add_declaration() {
    auto index = external_info.size();
    external_info.push_back({ .is_axiom = false });
    return index;
  }
}
