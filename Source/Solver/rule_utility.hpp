#ifndef RULE_UTILITY_HPP
#define RULE_UTILITY_HPP

#include "RuleStructures/pattern_tree_impl.hpp"
#include "../Compiler/new_instructions.hpp"
#include "../NewExpression/on_arena_utility.hpp"
#include "stack.hpp"

namespace solver {
  /*
    Important: These tools assume that every argument in the
    local context is applied (in order) to the declaration at
    the head. This should be enforced at a syntax level, and
    cannot be detected by this code.
  */
  struct FoldedPattern {
    new_expression::OwnedExpression head;
    std::size_t stack_arg_count;
    std::size_t capture_count;
    std::vector<pattern_node::PatternNode> matches;
    //represents pattern head arg_0 arg_1 ... arg_n matches[0] ... matches[m-1]
  };
  struct FlatPatternShard {
    std::size_t matched_arg_index; //in "true" indexing of pattern args
    new_expression::OwnedExpression match_head;
    std::size_t capture_count;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct FlatPatternCheck {
    std::size_t arg_index; //always some matched argument = something else
    pattern_expr::PatternExpr expected_value;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct FlatPattern {
    new_expression::OwnedExpression head;
    std::size_t stack_arg_count; //# of args that *must* be matched first
    std::size_t arg_count;
    std::vector<FlatPatternShard> shards;
    std::vector<new_expression::OwnedExpression> captures; //what to push on to locals stack
    std::vector<FlatPatternCheck> checks;
    static constexpr auto part_info = mdb::parts::simple<6>;
  };
  struct RequiredEquality {
    new_expression::OwnedExpression lhs;
    new_expression::OwnedExpression rhs;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct PatternExecutionResult {
    new_expression::Pattern pattern;
    new_expression::OwnedExpression type_of_pattern;
    std::vector<new_expression::OwnedExpression> captures; //what to push on to locals stack
    std::vector<std::pair<new_expression::OwnedExpression, new_expression::OwnedExpression> > checks;
    stack::Stack pattern_stack;
    static constexpr auto part_info = mdb::parts::simple<5>;
  };

  struct PatternResolveInterface {
    mdb::function<new_expression::OwnedExpression(std::uint64_t)> lookup_local;
    mdb::function<new_expression::OwnedExpression(std::uint64_t)> lookup_embed;
  };
  pattern_expr::PatternExpr resolve_pattern(compiler::new_instruction::output::archive_part::Pattern const&, PatternResolveInterface const& interface);
  FoldedPattern normalize_pattern(new_expression::Arena&, pattern_expr::PatternExpr, std::size_t capture_count);
  FlatPattern flatten_pattern(new_expression::Arena&, FoldedPattern);
  PatternExecutionResult execute_pattern(new_expression::Arena& arena, stack::Stack stack, new_expression::WeakExpression arrow, FlatPattern);
}

#endif
