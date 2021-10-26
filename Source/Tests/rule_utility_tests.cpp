#include "../Solver/rule_utility.hpp"
#include "../Utility/vector_utility.hpp"
#include "../Solver/manager.hpp" //for BasicContext
#include <catch.hpp>

using namespace solver;

TEST_CASE("Pattern normalization works on lambda functions.") {
  new_expression::Arena arena;
  SECTION("normalize_pattern works on a direct definition of a lambda with two captures.") {
    auto head = arena.declaration();
    auto pattern = archive(pattern_expr::Apply {
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(head)},
        pattern_expr::Capture{0}
      },
      pattern_expr::Capture{1}
    });
    auto normalized = normalize_pattern(arena, {.primary_pattern = std::move(pattern)}, 2).get_value().output;
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 0);
    REQUIRE(normalized.capture_count == 2);
    REQUIRE(normalized.matches.size() == 2);
    REQUIRE(normalized.matches[0].root().holds_capture());
    REQUIRE(normalized.matches[0].root().get_capture().capture_index == 0);
    REQUIRE(normalized.matches[1].root().holds_capture());
    REQUIRE(normalized.matches[1].root().get_capture().capture_index == 1);
    destroy_from_arena(arena, head, normalized);
  }
  SECTION("normalize_pattern works on a definition of a lambda with a context argument and a captures.") {
    auto head = arena.declaration();
    auto pattern = archive(pattern_expr::Apply {
      pattern_expr::Embed{arena.apply(
        arena.copy(head),
        arena.argument(0)
      )},
      pattern_expr::Capture{0}
    });
    auto normalized = normalize_pattern(arena, {.primary_pattern = std::move(pattern)}, 1).get_value().output;
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 1);
    REQUIRE(normalized.capture_count == 1);
    REQUIRE(normalized.matches.size() == 1);
    REQUIRE(normalized.matches[0].root().holds_capture());
    REQUIRE(normalized.matches[0].root().get_capture().capture_index == 0);
    destroy_from_arena(arena, head, normalized);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}

TEST_CASE("Pattern normalization destructures axioms.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto ax = arena.axiom();
    auto pattern = archive(pattern_expr::Apply {
      pattern_expr::Embed{arena.copy(head)},
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(ax)},
        pattern_expr::Capture{0}
      }
    });
    auto normalized = normalize_pattern(arena, {.primary_pattern = std::move(pattern)}, 1).get_value().output;
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 0);
    REQUIRE(normalized.capture_count == 1);
    REQUIRE(normalized.matches.size() == 1);
    REQUIRE(normalized.matches[0].root().holds_apply());
    auto& apply = normalized.matches[0].root().get_apply();
    REQUIRE(apply.head == ax);
    REQUIRE(apply.args.size() == 1);
    REQUIRE(apply.args[0].holds_capture());
    REQUIRE(apply.args[0].get_capture().capture_index == 0);
    destroy_from_arena(arena, normalized, head, ax);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern normalization does not destructure declarations.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto decl = arena.declaration();
    auto pattern = archive(pattern_expr::Apply {
      pattern_expr::Embed{arena.copy(head)},
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(decl)},
        pattern_expr::Capture{0}
      }
    });
    auto normalized = normalize_pattern(arena, {.primary_pattern = std::move(pattern)}, 1).get_value().output;
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 0);
    REQUIRE(normalized.capture_count == 1);
    REQUIRE(normalized.matches.size() == 1);
    /*Expect: pattern_node::Check{
      pattern_expr::Apply {
        pattern_expr::Embed{arena.copy(decl)},
        pattern_expr::Capture{0}
      }
    };*/
    REQUIRE(normalized.matches[0].root().holds_check());
    auto const& check = normalized.matches[0].root().get_check();
    REQUIRE(check.desired_equality.holds_apply());
    auto const& check_apply = check.desired_equality.get_apply();
    REQUIRE(check_apply.lhs.holds_embed());
    REQUIRE(check_apply.lhs.get_embed().value == decl);
    REQUIRE(check_apply.rhs.holds_capture());
    REQUIRE(check_apply.rhs.get_capture().capture_index == 0);
    destroy_from_arena(arena, head, decl, normalized);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern normalization turns stray arguments into checks.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto ax = arena.axiom();
    auto pattern = archive(pattern_expr::Apply {
      pattern_expr::Embed{arena.apply(
        arena.copy(head),
        arena.argument(0)
      )},
      pattern_expr::Embed{arena.apply(
        arena.copy(ax),
        arena.argument(0)
      )}
    });
    auto normalized = normalize_pattern(arena, {.primary_pattern = std::move(pattern)}, 0).get_value().output;
    REQUIRE(normalized.head == head);
    REQUIRE(normalized.stack_arg_count == 1);
    REQUIRE(normalized.capture_count == 0);
    REQUIRE(normalized.matches.size() == 1);
    REQUIRE(normalized.matches[0].root().holds_apply());
    auto& apply = normalized.matches[0].root().get_apply();
    REQUIRE(apply.head == ax);
    REQUIRE(apply.args.size() == 1);
    /* Expect: pattern_node::Check{
      pattern_expr::Embed{arena.argument(0)}
    }; */
    auto arg_zero = arena.argument(0);
    REQUIRE(apply.args[0].holds_check());
    REQUIRE(apply.args[0].get_check().desired_equality.holds_embed());
    REQUIRE(apply.args[0].get_check().desired_equality.get_embed().value == arg_zero);
    destroy_from_arena(arena, head, ax, arg_zero, normalized);
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
      .matches = mdb::make_vector(
        archive(pattern_node::Capture{0}),
        archive(pattern_node::Capture{1})
      )
    };
    auto flat_result = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat_result.holds_success());
    auto& flat = flat_result.get_value().output;
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.size() == 2);
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[0]));
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[1]));
    REQUIRE(flat.captures.size() == 2);
    auto arg = arena.argument(0);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    arg = arena.argument(1);
    REQUIRE(flat.captures[1] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.empty());
    destroy_from_arena(arena, head, flat);
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
      .matches = mdb::make_vector(
        archive(pattern_node::Capture{0}),
        archive(pattern_node::Capture{0})
      )
    };
    auto flat_result = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat_result.holds_success());
    auto& flat = flat_result.get_value().output;
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.size() == 2);
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[0]));
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[1]));
    REQUIRE(flat.captures.size() == 1);
    auto arg = arena.argument(0);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.size() == 1);
    REQUIRE(flat.checks[0].arg_index == 1);
    REQUIRE(flat.checks[0].expected_value == pattern_expr::Capture{0});
    destroy_from_arena(arena, head, flat);
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
      .matches = mdb::make_vector(
        archive(pattern_node::Check{
          pattern_expr::Apply{
            pattern_expr::Embed{arena.copy(decl)},
            pattern_expr::Capture{0}
          }
        }),
        archive(pattern_node::Capture{0})
      )
    };
    auto flat_result = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat_result.holds_success());
    auto& flat = flat_result.get_value().output;
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.size() == 2);
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[0]));
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[1]));
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
    destroy_from_arena(arena, head, decl, expected_check, flat);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Pattern flattening creates shards for nested destructuring.") {
  new_expression::Arena arena;
  {
    auto head = arena.declaration();
    auto ax = arena.axiom();
    FoldedPattern pat{
      .head = arena.copy(head),
      .stack_arg_count = 0,
      .capture_count = 2,
      .matches = mdb::make_vector(
        archive(pattern_node::Apply{
          .args = mdb::make_vector<pattern_node::PatternNode>(
            pattern_node::Capture{0}
          ),
          .head = arena.copy(ax)
        }),
        archive(pattern_node::Capture{1})
      )
    };
    auto flat_result = flatten_pattern(arena, std::move(pat));
    REQUIRE(flat_result.holds_success());
    auto& flat = flat_result.get_value().output;
    REQUIRE(flat.head == head);
    REQUIRE(flat.stack_arg_count == 0);
    REQUIRE(flat.shards.size() == 3);
    /*
    Shards are: Pull, Match first arg, Pull. No other orders are allowable.
    */
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[0]));
    REQUIRE(std::holds_alternative<FlatPatternMatch>(flat.shards[1]));
    auto& shard_match = std::get<FlatPatternMatch>(flat.shards[1]);
    REQUIRE(std::holds_alternative<FlatPatternMatchArg>(shard_match.matched_expr)); //implicit matches should be represented by args
    REQUIRE(std::get<FlatPatternMatchArg>(shard_match.matched_expr).matched_arg_index == 0);
    REQUIRE(shard_match.match_head == ax);
    REQUIRE(shard_match.capture_count == 1);
    REQUIRE(std::holds_alternative<FlatPatternPullArgument>(flat.shards[2]));
    REQUIRE(flat.captures.size() == 2);
    auto arg = arena.argument(1);
    REQUIRE(flat.captures[0] == arg);
    arena.drop(std::move(arg));
    arg = arena.argument(2);
    REQUIRE(flat.captures[1] == arg);
    arena.drop(std::move(arg));
    REQUIRE(flat.checks.empty());
    destroy_from_arena(arena, head, ax, flat);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
/*
  No tests for resolve_pattern yet
*/
/*
namespace {
  stack::Stack get_empty_stack_for(solver::BasicContext& context) {
    return stack::Stack::empty({
      .type = context.primitives.type,
      .arrow = context.primitives.arrow,
      .id = context.primitives.id,
      .constant = context.primitives.constant,
      .type_family = context.primitives.type_family,
      .arena = context.arena,
      .rule_collector = context.rule_collector,
      .type_collector = context.type_collector,
      .register_type = [&context](new_expression::WeakExpression, new_expression::OwnedExpression t) {
        context.arena.drop(std::move(t));
      },
      .register_declaration = [&context](new_expression::WeakExpression expr) {
        context.rule_collector.register_declaration(expr);
      },
      .add_rule = [&context](new_expression::Rule rule) {
        context.rule_collector.add_rule(std::move(rule));
      }
    });
  }
}

TEST_CASE("If f : Nat -> Type, then a lambda pattern for f gives an appropriate stack.") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    auto stack = get_empty_stack_for(context);
    auto nat = arena.axiom();
    context.type_collector.type_of_primitive.set(
      nat,
      arena.copy(context.primitives.type)
    );
    auto f = arena.declaration();
    context.rule_collector.register_declaration(f);
    context.type_collector.type_of_primitive.set(
      f,
      arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(nat),
        arena.apply(
          arena.copy(context.primitives.constant),
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.type),
          arena.copy(nat)
        )
      )
    );
    FlatPattern pattern{
      .head = arena.copy(f),
      .stack_arg_count = 0,
      .arg_count = 1,
      .captures = mdb::make_vector(arena.argument(0))
    };
    auto result = execute_pattern(
      arena,
      stack,
      context.primitives.arrow,
      std::move(pattern)
    );
    REQUIRE(result.pattern.head == f);
    REQUIRE(result.pattern.body.args_captured == 1);
    REQUIRE(result.pattern.body.sub_matches.empty());
    REQUIRE(result.pattern.body.data_checks.empty());
    auto reduced_type_of_pattern = stack.reduce(arena.copy(result.type_of_pattern));
    REQUIRE(reduced_type_of_pattern == context.primitives.type);
    REQUIRE(result.captures.size() == 1);
    auto arg = arena.argument(0);
    REQUIRE(result.captures[0] == arg);
    REQUIRE(result.checks.empty());
    REQUIRE(result.pattern_stack.depth() == 1);
    auto arg_0_type = stack.reduce(result.pattern_stack.type_of_arg(0));
    REQUIRE(arg_0_type == nat);
    destroy_from_arena(arena, nat, f, reduced_type_of_pattern, arg, arg_0_type, result);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("If f : Nat -> Type, then a lambda pattern for f on a context with Nat gives appropriate results.") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    auto stack = get_empty_stack_for(context);
    auto nat = arena.axiom();
    context.type_collector.type_of_primitive.set(
      nat,
      arena.copy(context.primitives.type)
    );
    stack = stack.extend(arena.copy(nat));
    auto f = arena.declaration();
    context.rule_collector.register_declaration(f);
    context.type_collector.type_of_primitive.set(
      f,
      arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(nat),
        arena.apply(
          arena.copy(context.primitives.constant),
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.type),
          arena.copy(nat)
        )
      )
    );
    FlatPattern pattern{
      .head = arena.copy(f),
      .stack_arg_count = 1,
      .arg_count = 1
    };
    auto result = execute_pattern(
      arena,
      stack,
      context.primitives.arrow,
      std::move(pattern)
    );
    REQUIRE(result.pattern.head == f);
    REQUIRE(result.pattern.body.args_captured == 1);
    REQUIRE(result.pattern.body.sub_matches.empty());
    REQUIRE(result.pattern.body.data_checks.empty());
    auto reduced_type_of_pattern = stack.reduce(arena.copy(result.type_of_pattern));
    REQUIRE(reduced_type_of_pattern == context.primitives.type);
    REQUIRE(result.captures.empty());
    REQUIRE(result.checks.empty());
    REQUIRE(result.pattern_stack.depth() == 1);
    auto arg_0_type = stack.reduce(result.pattern_stack.type_of_arg(0));
    REQUIRE(arg_0_type == nat);
    destroy_from_arena(arena, nat, f, reduced_type_of_pattern, arg_0_type, result);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("If f : (n : Nat) -> Family n -> Type and ax : Family zero, then f $0 $1 where $1 = ax gives a context where $0 = zero.") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    auto stack = get_empty_stack_for(context);

    auto nat = arena.axiom();
    context.type_collector.type_of_primitive.set(
      nat,
      arena.copy(context.primitives.type)
    );
    auto fam = arena.axiom();
    context.type_collector.type_of_primitive.set(
      fam,
      arena.apply(
        arena.copy(context.primitives.type_family),
        arena.copy(nat)
      )
    );
    auto zero = arena.axiom();
    context.type_collector.type_of_primitive.set(
      zero,
      arena.copy(nat)
    );
    auto ax = arena.axiom();
    context.type_collector.type_of_primitive.set(
      ax,
      arena.apply(
        arena.copy(fam),
        arena.copy(zero)
      )
    );
    auto f = arena.declaration();
    context.rule_collector.register_declaration(f);
    auto f_codomain = arena.declaration();
    context.rule_collector.register_declaration(f_codomain);
    context.type_collector.type_of_primitive.set(
      f_codomain,
      arena.apply(
        arena.copy(context.primitives.type_family),
        arena.copy(nat)
      )
    );
    context.rule_collector.add_rule({
      .pattern = lambda_pattern(arena.copy(f_codomain), 1),
      .replacement = arena.apply(
        arena.copy(context.primitives.arrow),
        arena.apply(
          arena.copy(fam),
          arena.argument(0)
        ),
        arena.apply(
          arena.copy(context.primitives.constant),
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.type),
          arena.apply(
            arena.copy(fam),
            arena.argument(0)
          )
        )
      )
    });
    context.type_collector.type_of_primitive.set(
      f,
      arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(nat),
        arena.copy(f_codomain)
      )
    );
    FlatPattern pattern{
      .head = arena.copy(f),
      .stack_arg_count = 0,
      .arg_count = 2,
      .shards = mdb::make_vector(
        FlatPatternShard{
          .matched_arg_index = 1,
          .match_head = arena.copy(ax),
          .capture_count = 0
        }
      )
    };
    auto result = execute_pattern(
      arena,
      stack,
      context.primitives.arrow,
      std::move(pattern)
    );
    REQUIRE(result.pattern.head == f);
    REQUIRE(result.pattern.body.args_captured == 2);
    REQUIRE(result.pattern.body.sub_matches.size() == 1);
    auto expected_sub = arena.argument(1);
    REQUIRE(result.pattern.body.sub_matches[0].substitution == expected_sub);
    REQUIRE(result.pattern.body.sub_matches[0].expected_head == ax);
    REQUIRE(result.pattern.body.sub_matches[0].args_captured == 0);
    REQUIRE(result.pattern.body.data_checks.empty());
    auto reduced_type_of_pattern = result.pattern_stack.reduce(arena.copy(result.type_of_pattern));
    auto fam_zero = result.pattern_stack.reduce(arena.apply(
      arena.copy(fam),
      arena.copy(zero)
    ));
    REQUIRE(reduced_type_of_pattern == context.primitives.type);
    REQUIRE(result.captures.size() == 0);
    REQUIRE(result.checks.empty());
    REQUIRE(result.pattern_stack.depth() == 2);
    auto arg_0_type = result.pattern_stack.reduce(result.pattern_stack.type_of_arg(0));
    REQUIRE(arg_0_type == nat);
    auto arg_1_type = result.pattern_stack.reduce(result.pattern_stack.type_of_arg(1));
    REQUIRE(arg_1_type == fam_zero);
    destroy_from_arena(arena,
      nat, zero, fam, f, f_codomain, ax, expected_sub, reduced_type_of_pattern,
      fam_zero, arg_0_type, arg_1_type, result
    );
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
*/
