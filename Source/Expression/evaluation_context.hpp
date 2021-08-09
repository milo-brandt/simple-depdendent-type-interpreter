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
    std::uint64_t arrow_type_codomain;
    std::uint64_t type_constant_function;
    std::uint64_t arrow_type_codomain_codomain;
    tree::Tree arrow_type();
  };
  struct Context {
    std::vector<Rule> rules;
    std::vector<ExternalInfo> external_info;
    Primitives primitives;
    Context();
    std::uint64_t name_expression(tree::Tree);
    std::uint64_t add_axiom();
    std::uint64_t add_declaration();
    tree::Tree reduce(tree::Tree tree);
    tree::Tree reduce_once_at_root(tree::Tree tree);
  };
}

#endif
