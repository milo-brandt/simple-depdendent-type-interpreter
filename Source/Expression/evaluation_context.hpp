#ifndef EVALUATION_CONTEXT_HPP
#define EVALUATION_CONTEXT_HPP

#include "expression_tree.hpp"

namespace expression {
  struct Rule {
    pattern::Pattern pattern;
    tree::Expression replacement;
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
    tree::Expression arrow_type();
  };
  struct Context {
    std::vector<Rule> rules;
    std::vector<ExternalInfo> external_info;
    Primitives primitives;
    Context();
    std::uint64_t name_expression(tree::Expression);
    std::uint64_t add_axiom();
    std::uint64_t add_declaration();
    tree::Expression reduce(tree::Expression tree);
    tree::Expression reduce_once_at_root(tree::Expression tree);
  };
}

#endif
