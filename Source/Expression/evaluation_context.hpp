#ifndef EVALUATION_CONTEXT_HPP
#define EVALUATION_CONTEXT_HPP

#include "expression_tree.hpp"

namespace expression {
  struct Rule {
    pattern::Tree pattern;
    tree::Tree replacement;
  };
  struct ExternalInfo {
    bool is_axiom;
  };
  struct Primitives {
    std::uint64_t type;
    std::uint64_t arrow;
  };
  struct Context {
    std::vector<Rule> rules;
    std::vector<ExternalInfo> external_info;
    Primitives primitives;
    Context();
    std::uint64_t name_expression(tree::Tree);
    tree::Tree reduce(tree::Tree tree);
  };
}

#endif
