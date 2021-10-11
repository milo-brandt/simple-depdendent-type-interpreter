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
TEST_CASE("EvaluationContext can deal with $0 = $1 equality.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
    auto equal_err = evaluator.assume_equal(
      arena.argument(0), arena.argument(1)
    );
    REQUIRE(!equal_err);
    SECTION("Reducing $0 and $1 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      auto result_2 = evaluator.reduce(arena.argument(1));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
    SECTION("Reducing (axiom $0) and (axiom $1) gives the same value.") {
      auto func = arena.axiom();

      auto result_1 = evaluator.reduce(arena.apply(arena.copy(func), arena.argument(0)));
      auto result_2 = evaluator.reduce(arena.apply(std::move(func), arena.argument(1)));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can deal with $0 = $0 equality.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
    auto equal_err = evaluator.assume_equal(
      arena.argument(0), arena.argument(0)
    );
    REQUIRE(!equal_err);
    SECTION("Reducing $0 succeeds.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      REQUIRE(result_1.holds_success());
      arena.drop(std::move(result_1.get_value()));
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can deal with $0 = $1 and $1 = $2 equalities.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
    auto equal_err = evaluator.assume_equal(
      arena.argument(0), arena.argument(1)
    );
    REQUIRE(!equal_err);
    equal_err = evaluator.assume_equal(
       arena.argument(1), arena.argument(2)
    );
    REQUIRE(!equal_err);
    SECTION("Reducing $0 and $1 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      auto result_2 = evaluator.reduce(arena.argument(1));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
    SECTION("Reducing $1 and $2 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(1));
      auto result_2 = evaluator.reduce(arena.argument(2));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
    SECTION("Reducing $0 and $2 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      auto result_2 = evaluator.reduce(arena.argument(2));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can deal with $0 = (axiom $1 $2) equality.") {
  //this test case is to ensure proper expansion of axioms; it's covered in the
  //next case too, but this is an easier to debug case.
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
    auto ax = arena.axiom();
    auto expr = arena.apply(
      arena.apply(
        std::move(ax),
        arena.argument(1)
      ),
      arena.argument(2)
    );
    auto equal_err = evaluator.assume_equal(
      arena.argument(0),
      arena.copy(expr)
    );
    REQUIRE(!equal_err);
    auto result_1 = evaluator.reduce(arena.argument(0));
    auto result_2 = evaluator.reduce(std::move(expr));
    REQUIRE(result_1.holds_success());
    REQUIRE(result_2.holds_success());
    REQUIRE(result_1.get_value() == result_2.get_value());
    arena.drop(std::move(result_1.get_value()));
    arena.drop(std::move(result_2.get_value()));
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can deal with (axiom $0 $1) = (axiom $2 $3) equality.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
    auto ax = arena.axiom();
    auto lhs = arena.apply(
      arena.apply(
        arena.copy(ax),
        arena.argument(0)
      ),
      arena.argument(1)
    );
    auto rhs = arena.apply(
      arena.apply(
        std::move(ax),
        arena.argument(2)
      ),
      arena.argument(3)
    );

    auto equal_err = evaluator.assume_equal(
      std::move(lhs),
      std::move(rhs)
    );
    REQUIRE(!equal_err);
    SECTION("Reducing $0 and $2 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      auto result_2 = evaluator.reduce(arena.argument(2));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
    SECTION("Reducing $1 and $3 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(1));
      auto result_2 = evaluator.reduce(arena.argument(3));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can deal with $4 = (axiom $0 $1) and $4 = (axiom $2 $3) equalities.") {
  Arena arena; //doubler
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto ax = arena.axiom();
    auto lhs = arena.apply(
      arena.apply(
        arena.copy(ax),
        arena.argument(0)
      ),
      arena.argument(1)
    );
    auto rhs = arena.apply(
      arena.apply(
        std::move(ax),
        arena.argument(2)
      ),
      arena.argument(3)
    );

    auto equal_err = evaluator.assume_equal(
      arena.argument(4), std::move(lhs)
    );
    REQUIRE(!equal_err);
    equal_err = evaluator.assume_equal(
      arena.argument(4), std::move(rhs)
    );
    REQUIRE(!equal_err);
    SECTION("Reducing $0 and $2 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      auto result_2 = evaluator.reduce(arena.argument(2));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
    SECTION("Reducing $1 and $3 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(1));
      auto result_2 = evaluator.reduce(arena.argument(3));
      REQUIRE(result_1.holds_success());
      REQUIRE(result_2.holds_success());
      REQUIRE(result_1.get_value() == result_2.get_value());
      arena.drop(std::move(result_1.get_value()));
      arena.drop(std::move(result_2.get_value()));
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext rejects $0 = (axiom $0) immediately.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
    auto equal_err = evaluator.assume_equal(
      arena.argument(0),
      arena.apply(
        arena.axiom(),
        arena.argument(0)
      )
    );
    REQUIRE(equal_err);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
