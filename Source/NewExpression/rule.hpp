#ifndef EXPRESSION_RULE_HPP
#define EXPRESSION_RULE_HPP

#include "arena.hpp"
#include "arena_container.hpp"
#include "on_arena_utility.hpp"
#include <vector>

namespace new_expression {
  struct PatternMatch {
    OwnedExpression substitution;
    OwnedExpression expected_head;
    std::size_t args_captured;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct DataChecks {
    std::size_t capture_index;
    std::uint64_t expected_type;
  };
  struct PatternBody {
    std::size_t args_captured;
    std::vector<PatternMatch> sub_matches;
    std::vector<DataChecks> data_checks;
    static constexpr auto part_info = mdb::parts::simple<3>;
  }; //output is some list of captures, if successful
  struct Pattern {
    OwnedExpression head;
    PatternBody body;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  inline new_expression::Pattern lambda_pattern(OwnedExpression head, std::size_t args) {
    return {
      .head = std::move(head),
      .body = { .args_captured = args }
    };
  }
  struct Rule {
    Pattern pattern;
    OwnedExpression replacement;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct RuleBody {
    PatternBody pattern_body;
    OwnedExpression replacement;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct DeclarationInfo {
    bool is_indeterminate;
    std::vector<RuleBody> rules;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  class RuleCollector {
    Arena* p_arena;
    WeakKeyMap<DeclarationInfo, PartDestroyer> p_declaration_info;
  public:
    void register_declaration(WeakExpression, bool indeterminate = false);
    DeclarationInfo const& declaration_info(WeakExpression) const;
    RuleCollector(Arena&);
    mdb::Event<> new_rule;
    void add_rule(Rule);
  };
}

#endif
