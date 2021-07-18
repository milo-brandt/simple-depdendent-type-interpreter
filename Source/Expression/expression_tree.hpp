#ifndef EXPRESSION_TREE_HPP
#define EXPRESSION_TREE_HPP

#include "expression_tree_impl.hpp"
#include "pattern_tree_impl.hpp"

namespace expression {
  bool term_matches(tree::Tree const&, pattern::Tree const&);
  struct NoMatchException : std::runtime_error {
    NoMatchException():std::runtime_error("Attempted to destructure non-existant match.") {}
  };

  std::vector<tree::Tree> destructure_match(tree::Tree, pattern::Tree const&); //throws if not matching
  std::vector<path::Path> captures_of_pattern(pattern::Tree const&);
  std::vector<path::Path> find_all_matches(tree::Tree const&, pattern::Tree const&);

  struct NotEnoughArguments : std::runtime_error {
    NotEnoughArguments():std::runtime_error("Not enough arguments to substitute into replacement.") {}
  };

  tree::Tree substitute_into_replacement(std::vector<tree::Tree> const& terms, tree::Tree const& replacement); //args are replaced
  void replace_with_substitution_at(tree::Tree& term, path::Path const& path, pattern::Tree const& pattern, tree::Tree const& replacement);
}

#endif
