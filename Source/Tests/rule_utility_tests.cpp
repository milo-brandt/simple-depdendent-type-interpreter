#include "../Solver/rule_utility.hpp"
#include "../Utility/vector_utility.hpp"
#include <catch.hpp>

using namespace solver;

void destroy_pattern_expr(new_expression::Arena& arena, pattern_expr::PatternExpr& expr) {
  expr.visit(mdb::overloaded{
    [&](pattern_expr::Apply& apply) {
      destroy_pattern_expr(arena, apply.lhs);
      destroy_pattern_expr(arena, apply.rhs);
    },
    [&](pattern_expr::Embed& embed) {
      arena.drop(std::move(embed.value));
    },
    [&](pattern_expr::Capture&) {},
    [&](pattern_expr::Hole&) {}
  });
}
void destroy_pattern_node(new_expression::Arena& arena, pattern_node::PatternNode& node) {
  node.visit(mdb::overloaded{
    [&](pattern_node::Apply& apply) {
      arena.drop(std::move(apply.head));
      for(auto& arg : apply.args) {
        destroy_pattern_node(arena, arg);
      }
    },
    [&](pattern_node::Capture&) {},
    [&](pattern_node::Check& check) {
      destroy_pattern_expr(arena, check.desired_equality);
    },
    [&](pattern_node::Ignore&) {}
  });
}
void destroy_folded_pattern(new_expression::Arena& arena, FoldedPattern& pattern) {
  arena.drop(std::move(pattern.head));
  for(auto& match : pattern.matches) {
    destroy_pattern_node(arena, match);
  }
}
void destroy_flat_pattern(new_expression::Arena& arena, FlatPattern& pattern) {
  destroy_from_arena(arena, pattern.head, pattern.captures, pattern.shards);
  for(auto& check : pattern.checks) {
    destroy_pattern_expr(arena, check.expected_value);
  }
}

TEST_CASE("Pattern normalization works on lambda functions.") {
  new_expression::Arena arena;
  SECTION("normalize_pattern works on a direct definition of a lambda with two captures.") {
    auto head = arena.declaration();
    auto pattern = pattern_expr::Apply {
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(head)},
        pattern_expr::Capture{0}
      },
      pattern_expr::Capture{1}
    };
    auto normalized = normalize_pattern(arena, std::move(pattern), 2);
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 0);
    REQUIRE(normalized.capture_count == 2);
    REQUIRE(normalized.matches.size() == 2);
    REQUIRE(normalized.matches[0] == pattern_node::Capture{0});
    REQUIRE(normalized.matches[1] == pattern_node::Capture{1});
    destroy_folded_pattern(arena, normalized);
    destroy_from_arena(arena, head);
  }
  SECTION("normalize_pattern works on a definition of a lambda with a context argument and a captures.") {
    auto head = arena.declaration();
    auto pattern = pattern_expr::Apply {
      pattern_expr::Embed{arena.apply(
        arena.copy(head),
        arena.argument(0)
      )},
      pattern_expr::Capture{0}
    };
    auto normalized = normalize_pattern(arena, std::move(pattern), 1);
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 1);
    REQUIRE(normalized.capture_count == 1);
    REQUIRE(normalized.matches.size() == 1);
    REQUIRE(normalized.matches[0] == pattern_node::Capture{0});
    destroy_folded_pattern(arena, normalized);
    destroy_from_arena(arena, head);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern normalization destructures axioms.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto ax = arena.axiom();
    auto pattern = pattern_expr::Apply {
      pattern_expr::Embed{arena.copy(head)},
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(ax)},
        pattern_expr::Capture{0}
      }
    };
    auto normalized = normalize_pattern(arena, std::move(pattern), 1);
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 0);
    REQUIRE(normalized.capture_count == 1);
    REQUIRE(normalized.matches.size() == 1);
    REQUIRE(normalized.matches[0].holds_apply());
    auto& apply = normalized.matches[0].get_apply();
    REQUIRE(apply.head == ax);
    REQUIRE(apply.args.size() == 1);
    REQUIRE(apply.args[0] == pattern_node::Capture{0});
    destroy_folded_pattern(arena, normalized);
    destroy_from_arena(arena, head, ax);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern normalization does not destructure declarations.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto decl = arena.declaration();
    auto pattern = pattern_expr::Apply {
      pattern_expr::Embed{arena.copy(head)},
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(decl)},
        pattern_expr::Capture{0}
      }
    };
    auto normalized = normalize_pattern(arena, std::move(pattern), 1);
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 0);
    REQUIRE(normalized.capture_count == 1);
    REQUIRE(normalized.matches.size() == 1);
    pattern_node::PatternNode expected = pattern_node::Check{
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(decl)},
        pattern_expr::Capture{0}
      }
    };
    REQUIRE(normalized.matches[0] == expected);
    destroy_folded_pattern(arena, normalized);
    destroy_pattern_node(arena, expected);
    destroy_from_arena(arena, head, decl);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern normalization turns stray arguments into checks.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto ax = arena.axiom();
    auto pattern = pattern_expr::Apply {
      pattern_expr::Embed{arena.apply(
        arena.copy(head),
        arena.argument(0)
      )},
      pattern_expr::Embed{arena.apply(
        arena.copy(ax),
        arena.argument(0)
      )}
    };
    auto normalized = normalize_pattern(arena, std::move(pattern), 0);
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 1);
    REQUIRE(normalized.capture_count == 0);
    REQUIRE(normalized.matches.size() == 1);
    REQUIRE(normalized.matches[0].holds_apply());
    auto& apply = normalized.matches[0].get_apply();
    REQUIRE(apply.head == ax);
    REQUIRE(apply.args.size() == 1);
    pattern_node::PatternNode expected = pattern_node::Check{
      pattern_expr::Embed{arena.argument(0)}
    };
    REQUIRE(apply.args[0] == expected);
    destroy_folded_pattern(arena, normalized);
    destroy_pattern_node(arena, expected);
    destroy_from_arena(arena, head, ax);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern flattening handles lambda patterns") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    FoldedPattern pat{
      .head = arena.copy(head),
      .stack_arg_count = 0,
      .capture_count = 2,
      .matches = mdb::make_vector<pattern_node::PatternNode>(
        pattern_node::Capture{0},
        pattern_node::Capture{1}
      )
    };
    auto flat = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.empty());
    REQUIRE(flat.captures.size() == 2);
    auto arg = arena.argument(0);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    arg = arena.argument(1);
    REQUIRE(flat.captures[1] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.empty());
    destroy_flat_pattern(arena, flat);
    destroy_from_arena(arena, head);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern flattening creates implicit checks for equal captures.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    FoldedPattern pat{
      .head = arena.copy(head),
      .stack_arg_count = 0,
      .capture_count = 1,
      .matches = mdb::make_vector<pattern_node::PatternNode>(
        pattern_node::Capture{0},
        pattern_node::Capture{0}
      )
    };
    auto flat = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.empty());
    REQUIRE(flat.captures.size() == 1);
    auto arg = arena.argument(0);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.size() == 1);
    REQUIRE(flat.checks[0].arg_index == 1);
    REQUIRE(flat.checks[0].expected_value == pattern_expr::Capture{0});
    destroy_flat_pattern(arena, flat);
    destroy_from_arena(arena, head);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern flattening creates checks for explicit checks.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto decl = arena.declaration();
    FoldedPattern pat{
      .head = arena.copy(head),
      .stack_arg_count = 0,
      .capture_count = 1,
      .matches = mdb::make_vector<pattern_node::PatternNode>(
        pattern_node::Check{
          pattern_expr::Apply{
            pattern_expr::Embed{arena.copy(decl)},
            pattern_expr::Capture{0}
          }
        },
        pattern_node::Capture{0}
      )
    };
    auto flat = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.empty());
    REQUIRE(flat.captures.size() == 1);
    auto arg = arena.argument(1);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.size() == 1);
    REQUIRE(flat.checks[0].arg_index == 0);
    pattern_expr::PatternExpr expected_check = pattern_expr::Apply{
      pattern_expr::Embed{arena.copy(decl)},
      pattern_expr::Capture{0}
    };
    REQUIRE(flat.checks[0].expected_value == expected_check);
    destroy_pattern_expr(arena, expected_check);
    destroy_flat_pattern(arena, flat);
    destroy_from_arena(arena, head, decl);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern flattening creates shards for nested checks.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto ax = arena.axiom();
    FoldedPattern pat{
      .head = arena.copy(head),
      .stack_arg_count = 0,
      .capture_count = 2,
      .matches = mdb::make_vector<pattern_node::PatternNode>(
        pattern_node::Apply{
          .args = mdb::make_vector<pattern_node::PatternNode>(
            pattern_node::Capture{0}
          ),
          .head = arena.copy(ax)
        },
        pattern_node::Capture{1}
      )
    };
    auto flat = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.size() == 1);
    REQUIRE(flat.shards[0].matched_arg_index == 0);
    REQUIRE(flat.shards[0].match_head == ax);
    REQUIRE(flat.shards[0].capture_count == 1);
    REQUIRE(flat.captures.size() == 2);
    auto arg = arena.argument(2);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    arg = arena.argument(1);
    REQUIRE(flat.captures[1] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.empty());
    destroy_flat_pattern(arena, flat);
    destroy_from_arena(arena, head, ax);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
/*
  No tests for resolve_pattern yet
*/
