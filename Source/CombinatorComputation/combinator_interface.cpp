#include "combinator_interface.hpp"
#include "matching.hpp"

namespace combinator {
  std::pair<InterfaceOutput, InterfaceState> process_term(Term const& term, std::vector<ReplacementRule> const& rules, mdb::function<void(std::stringstream&, TermLeaf const)> format_leaf) {
    auto [string, spans] = format::format_expression_tree(term, format_leaf);
    auto span_of = [&](mdb::TreePath const& path) {
      return mdb::read_tree_label(mdb::tree_at_path(spans, path));
    };
    auto left_part_of = [&](mdb::TreePath const& path) {
      auto const* tree = &mdb::tree_at_path(spans, path);
      while(auto* node = tree->get_if_node()) {
        tree = &std::get<1>(*node);
      }
      return tree->get_leaf();
    };
    std::pair<InterfaceOutput, InterfaceState> ret;
    auto& [output, state] = ret;
    output.string = std::move(string);
    for(auto const& rule : rules) {
      auto matches = find_all_matches(term, rule.pattern);
      auto captures = captures_of_pattern(rule.pattern);
      for(auto const& match_path : matches) {
        std::vector<format::Span> matches;
        for(auto const& capture_path : captures) {
          matches.push_back(span_of(mdb::append_path(match_path, capture_path)));
        }
        output.matches.push_back(Match{
          .trigger = left_part_of(match_path),
          .area = span_of(match_path),
          .matches = std::move(matches)
        });
        state.updates.push_back(make_substitution(term, match_path, rule.pattern, rule.replacement));
      }
    }
    return ret;
  }
}
