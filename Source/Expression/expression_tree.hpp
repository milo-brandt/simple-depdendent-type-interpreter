#ifndef EXPRESSION_TREE_HPP
#define EXPRESSION_TREE_HPP

#include "expression_tree_impl.hpp"
#include "pattern_tree_impl.hpp"
#include "data_pattern_tree_impl.hpp"
#include "../Utility/function.hpp"

namespace expression {
  bool term_matches(tree::Expression const&, pattern::Pattern const&);
  bool term_matches(tree::Expression const&, data_pattern::Pattern const&);
  struct NoMatchException : std::runtime_error {
    NoMatchException():std::runtime_error("Attempted to destructure non-existant match.") {}
  };

  tree::Expression trivial_replacement_for(pattern::Pattern const&); //mostly for printing!

  std::vector<tree::Expression> destructure_match(tree::Expression const&, data_pattern::Pattern const&); //throws if not matching
  std::vector<tree::Expression> destructure_match(tree::Expression const&, pattern::Pattern const&); //throws if not matching

  struct NotEnoughArguments : std::runtime_error {
    NotEnoughArguments():std::runtime_error("Not enough arguments to substitute into replacement.") {}
  };

  tree::Expression substitute_into_replacement(std::vector<tree::Expression> const& terms, tree::Expression const& replacement); //args are replaced
  std::optional<tree::Expression> remap_args(std::unordered_map<std::uint64_t, std::uint64_t> const& arg_map, tree::Expression const& target); //arg{i} is replaced by arg{arg_map[i]} if possible
  void replace_with_substitution_at(tree::Expression* position, pattern::Pattern const& pattern, tree::Expression const& replacement);
  std::uint64_t get_pattern_head(expression::pattern::Pattern const& pat);
  std::uint64_t get_pattern_head(expression::data_pattern::Pattern const& pat);
  std::uint64_t count_pattern_args(expression::pattern::Pattern const& pat);
  std::uint64_t count_pattern_args(expression::data_pattern::Pattern const& pat);

  struct TypedValue {
    tree::Expression value;
    tree::Expression type;
    friend bool operator==(TypedValue const& lhs, TypedValue const& rhs) {
      return lhs.value == rhs.value && lhs.type == rhs.type;
    }
  };

  struct Unfolding {
    tree::Expression head;
    std::vector<tree::Expression> args;
    tree::Expression fold() &&;
  };
  Unfolding unfold(tree::Expression);

  struct Rule {
    pattern::Pattern pattern;
    tree::Expression replacement;
  };
  struct DataRule {
    data_pattern::Pattern pattern;
    mdb::function<tree::Expression(std::vector<tree::Expression>)> replace;
  };

  indexed_pattern::Pattern index_pattern(pattern::Pattern const&);
  indexed_data_pattern::Pattern index_pattern(data_pattern::Pattern const&);

}

#endif
