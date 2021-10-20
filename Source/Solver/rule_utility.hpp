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

  /*
    A pattern such as
      f x y
        where g x = succ z
    Should be flattened as
      f $0 $1

  */
  struct RawPatternShard {
    std::vector<std::uint64_t> used_captures;
    pattern_expr::archive_root::PatternExpr pattern;
  };
  struct RawPattern {
    pattern_expr::archive_root::PatternExpr primary_pattern;
    std::vector<RawPatternShard> subpatterns;
  };
  struct FoldedPatternBacktrace {
    std::vector<pattern_normalization_locator::archive_root::PatternNode> matches;
    std::vector<pattern_normalization_locator::archive_root::PatternNode> subclause_matches;
  };
  struct FoldedSubclause {
    std::vector<std::uint64_t> used_captures;
    pattern_node::archive_root::PatternNode node; //must be an apply node
  };
  struct FoldedPattern {
    new_expression::OwnedExpression head;
    std::size_t stack_arg_count;
    std::size_t capture_count;
    std::vector<pattern_node::archive_root::PatternNode> matches;
    std::vector<FoldedSubclause> subclause_matches;
    //represents pattern head arg_0 arg_1 ... arg_n matches[0] ... matches[m-1]
  };
  struct FlatPatternMatchArg {
    std::size_t matched_arg_index; //in "true" indexing of pattern args
  };
  struct FlatPatternMatchSubexpression {
    std::vector<new_expression::OwnedExpression> requested_captures;
    std::size_t matched_subexpression;
  };
  using FlatPatternMatchExpr = std::variant<FlatPatternMatchArg, FlatPatternMatchSubexpression>;
  struct FlatPatternMatch {
    FlatPatternMatchExpr matched_expr;
    new_expression::OwnedExpression match_head;
    std::size_t capture_count;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct FlatPatternPullArgument {};
  using FlatPatternShard = std::variant<FlatPatternMatch, FlatPatternPullArgument>;
  struct FlatPatternMatchExplanation {
    pattern_node_archive_index::Apply source;
  };
  struct FlatPatternPullArgumentExplanation {};
  using FlatPatternShardExplanation = std::variant<FlatPatternMatchExplanation, FlatPatternPullArgumentExplanation>;
  struct FlatPatternDoubleCaptureCheck {
    pattern_node_archive_index::Capture primary_capture;
    pattern_node_archive_index::Capture secondary_capture;
  };
  struct FlatPatternInlineCheck {
    pattern_node_archive_index::Check source;
  };
  using FlatPatternCheckExplanation = std::variant<FlatPatternDoubleCaptureCheck, FlatPatternInlineCheck>;
  struct FlatPatternExplanation {
    std::vector<FlatPatternShardExplanation> shards;
    std::vector<FlatPatternCheckExplanation> checks;
  };
  struct FlatPatternCheck {
    std::size_t arg_index; //always some matched argument = something else
    pattern_expr::PatternExpr expected_value;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct FlatPattern {
    new_expression::OwnedExpression head; //does not include initial pulls for stack_arg_count
    std::size_t stack_arg_count; //# of args that *must* be matched first
    std::size_t arg_count;
    std::vector<FlatPatternShard> shards;
    std::vector<new_expression::OwnedExpression> captures; //what to push on to locals stack
    std::vector<FlatPatternCheck> checks;
    static constexpr auto part_info = mdb::parts::simple<6>;
  };
  struct PatternExecutionResult {
    new_expression::Pattern pattern;
    new_expression::OwnedExpression type_of_pattern;
    std::vector<new_expression::OwnedExpression> captures; //what to push on to locals stack
    std::vector<std::pair<new_expression::OwnedExpression, new_expression::OwnedExpression> > checks; //indexed same as prior checks
    stack::Stack pattern_stack;
    static constexpr auto part_info = mdb::parts::simple<5>;
  };

  struct PatternResolveInterface {
    mdb::function<new_expression::OwnedExpression(std::uint64_t)> lookup_local;
    mdb::function<new_expression::OwnedExpression(std::uint64_t)> lookup_embed;
  };
  struct ResolutionResult {
    pattern_expr::archive_root::PatternExpr output;
    pattern_resolution_locator::archive_root::PatternExpr locator;
  };
  ResolutionResult resolve_pattern(compiler::new_instruction::output::archive_part::Pattern const&, PatternResolveInterface const& interface);
  struct NormalizationResult {
    FoldedPattern output;
    FoldedPatternBacktrace locator;
  };
  NormalizationResult normalize_pattern(new_expression::Arena&, RawPattern raw, std::size_t capture_count);
  struct FlatResult {
    FlatPattern output;
    FlatPatternExplanation locator;
  };
  FlatResult flatten_pattern(new_expression::Arena&, FoldedPattern);
  struct PatternExecuteInterface {
    new_expression::Arena& arena;
    new_expression::WeakExpression arrow;
    stack::Stack stack;
    mdb::function<new_expression::OwnedExpression(std::uint64_t, stack::Stack, std::vector<new_expression::OwnedExpression>)> get_subexpression;
  };
  PatternExecutionResult execute_pattern(PatternExecuteInterface, FlatPattern);
}

#endif
