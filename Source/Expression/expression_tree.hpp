#ifndef EXPRESSION_TREE_HPP
#define EXPRESSION_TREE_HPP

#include "expression_tree_impl.hpp"
#include "pattern_tree_impl.hpp"
#include "../Utility/function.hpp"

namespace expression {
  bool term_matches(tree::Expression const&, pattern::Pattern const&);
  struct NoMatchException : std::runtime_error {
    NoMatchException():std::runtime_error("Attempted to destructure non-existant match.") {}
  };

  tree::Expression trivial_replacement_for(pattern::Pattern const&); //mostly for printing!

  std::vector<tree::Expression*> destructure_match_ref(tree::Expression&, pattern::Pattern const&); //throws if not matching
  std::vector<tree::Expression const*> destructure_match_ref(tree::Expression const&, pattern::Pattern const&); //throws if not matching
  std::vector<tree::Expression> destructure_match(tree::Expression, pattern::Pattern const&); //throws if not matching

  std::vector<tree::Expression*> find_all_matches(tree::Expression&, pattern::Pattern const&);
  std::vector<tree::Expression const*> find_all_matches(tree::Expression const&, pattern::Pattern const&);

  struct NotEnoughArguments : std::runtime_error {
    NotEnoughArguments():std::runtime_error("Not enough arguments to substitute into replacement.") {}
  };

  tree::Expression substitute_into_replacement(std::vector<tree::Expression> const& terms, tree::Expression const& replacement); //args are replaced
  tree::Expression substitute_into_replacement_callback(mdb::function<tree::Expression(std::uint64_t)>, tree::Expression const& replacement);
  void replace_with_substitution_at(tree::Expression* position, pattern::Pattern const& pattern, tree::Expression const& replacement);

  struct TypedValue {
    tree::Expression value;
    tree::Expression type;
  };

  struct RefUnfolding {
    tree::Expression const* head;
    std::vector<tree::Expression const*> args;
  };
  RefUnfolding unfold_ref(tree::Expression const&);
  struct Unfolding {
    tree::Expression head;
    std::vector<tree::Expression> args;
  };
  Unfolding unfold(tree::Expression);
}

#endif
