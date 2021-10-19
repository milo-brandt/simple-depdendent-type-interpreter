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
  struct DataCheck {
    std::size_t capture_index;
    std::uint64_t expected_type;
  };
  struct PullArgument {}; //pull from main expression
  using PatternStep = std::variant<PatternMatch, DataCheck, PullArgument>;
  struct PatternBody {
    std::size_t args_captured; //just for quick identification of if a rule could apply
    std::vector<PatternStep> steps;
    static constexpr auto part_info = mdb::parts::simple<2>;
  }; //output is some list of captures, if successful
  struct Pattern {
    OwnedExpression head;
    PatternBody body;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  inline new_expression::Pattern lambda_pattern(OwnedExpression head, std::size_t args) {
    std::vector<PatternStep> steps;
    for(std::size_t i = 0; i < args; ++i) {
      steps.push_back(PullArgument{});
    }
    return {
      .head = std::move(head),
      .body = {
        .args_captured = args,
        .steps = std::move(steps)
      }
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
