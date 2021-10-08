#include "../NewExpression/evaluation.hpp"
#include "../Utility/vector_utility.hpp"
#include <catch.hpp>

using namespace new_expression;

TEST_CASE("SimpleEvaluationContext can handle the double : Nat -> Nat function.") {
  Arena arena; //doubler
  {
    RuleCollector rules(arena);
    SimpleEvaluationContext evaluator(arena, rules);
    auto doubler = arena.declaration();
    auto zero = arena.axiom();
    auto succ = arena.axiom();

    rules.register_declaration(doubler);
    rules.add_rule(Rule{ //doubler zero = zero
      .pattern = {
        .head = arena.copy(doubler),
        .body = {
          .args_captured = 1,
          .sub_matches = mdb::make_vector(PatternMatch{
            .substitution = arena.argument(0),
            .expected_head = arena.copy(zero),
            .args_captured = 0
          })
        }
      },
      .replacement = arena.copy(zero)
    });
    rules.add_rule({ //doubler (succ n) = succ (succ n)
      .pattern = {
        .head = arena.copy(doubler),
        .body = {
          .args_captured = 1,
          .sub_matches = mdb::make_vector(PatternMatch{
            .substitution = arena.argument(0),
            .expected_head = arena.copy(succ),
            .args_captured = 1
          })
        }
      },
      .replacement = arena.apply(
        arena.copy(succ),
        arena.apply(
          arena.copy(succ),
          arena.apply(
            arena.copy(doubler),
            arena.argument(1)
          )
        )
      )
    });

    SECTION("doubler zero evaluates to zero") {
      auto result = evaluator.reduce_head(arena.apply(
        arena.copy(doubler),
        arena.copy(zero)
      ));
      REQUIRE(result == zero);
      destroy_from_arena(arena, result);
    }
    SECTION("doubler (doubler zero) evaluates to zero") {
      auto result = evaluator.reduce_head(arena.apply(
        arena.copy(doubler),
        arena.apply(
          arena.copy(doubler),
          arena.copy(zero)
        )
      ));
      REQUIRE(result == zero);
      destroy_from_arena(arena, result);
    }
    SECTION("doubler (succ zero) evaluates to succ (succ (doubler zero))") {
      auto result = evaluator.reduce_head(arena.apply(
        arena.copy(doubler),
        arena.apply(
          arena.copy(succ),
          arena.copy(zero)
        )
      ));
      auto expectation = arena.apply(
        arena.copy(succ),
        arena.apply(
          arena.copy(succ),
          arena.apply(
            arena.copy(doubler),
            arena.copy(zero)
          )
        )
      );
      REQUIRE(result == expectation);
      destroy_from_arena(arena, result, expectation);
    }
    SECTION("doubler (succ (succ zero)) evaluates to succ (succ (doubler (succ zero)))") {
      auto result = evaluator.reduce_head(arena.apply(
        arena.copy(doubler),
        arena.apply(
          arena.copy(succ),
          arena.apply(
            arena.copy(succ),
            arena.copy(zero)
          )
        )
      ));
      auto expectation = arena.apply(
        arena.copy(succ),
        arena.apply(
          arena.copy(succ),
          arena.apply(
            arena.copy(doubler),
            arena.apply(
              arena.copy(succ),
              arena.copy(zero)
            )
          )
        )
      );
      REQUIRE(result == expectation);
      destroy_from_arena(arena, result, expectation);
    }

    destroy_from_arena(arena, doubler, succ, zero);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
