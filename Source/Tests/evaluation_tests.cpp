#include "../NewExpression/evaluation.hpp"
#include "../Utility/vector_utility.hpp"
#include "../Utility/overloaded.hpp"
#include <catch.hpp>

using namespace new_expression;

bool is_free_from_conglomerates(Arena& arena, WeakExpression expr) {
  return arena.visit(expr, mdb::overloaded{
    [&](Apply const& apply) {
      return is_free_from_conglomerates(arena, apply.lhs)
          && is_free_from_conglomerates(arena, apply.rhs);
    },
    [&](Conglomerate const&) {
       return false;
    },
    [&](auto const&) {
      return true;
    }
  });
}

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
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("Reducing (axiom $0) and (axiom $1) gives the same value.") {
      auto func = arena.axiom();
      auto result_1 = evaluator.reduce(arena.apply(arena.copy(func), arena.argument(0)));
      auto result_2 = evaluator.reduce(arena.apply(std::move(func), arena.argument(1)));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("The copy operator for EvaluationContext works") {
      auto evaluator_2 = evaluator;
      auto result_1 = evaluator_2.reduce(arena.argument(0));
      auto result_2 = evaluator_2.reduce(arena.argument(1));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
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
      arena.drop(std::move(result_1));
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
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("Reducing $1 and $2 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(1));
      auto result_2 = evaluator.reduce(arena.argument(2));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("Reducing $0 and $2 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(0));
      auto result_2 = evaluator.reduce(arena.argument(2));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
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
    REQUIRE(result_1 == result_2);
    result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
    REQUIRE(is_free_from_conglomerates(arena, result_1));
    result_1 = evaluator.reduce(std::move(result_1));
    REQUIRE(result_1 == result_2);
    arena.drop(std::move(result_1));
    arena.drop(std::move(result_2));
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
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("Reducing $1 and $3 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(1));
      auto result_2 = evaluator.reduce(arena.argument(3));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
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
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("Reducing $1 and $3 gives the same value.") {
      auto result_1 = evaluator.reduce(arena.argument(1));
      auto result_2 = evaluator.reduce(arena.argument(3));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext rejects $0 = (axiom $0).") {
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
TEST_CASE("EvaluationContext rejects $0 $1 = axiom $0.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto ax = arena.axiom();
    auto equal_err = evaluator.assume_equal(
      arena.apply(
        arena.argument(0),
        arena.argument(1)
      ),
      arena.apply(
        std::move(ax),
        arena.argument(0)
      )
    );
    REQUIRE(equal_err);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext accepts $0 $1 = axiom $1.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto ax = arena.axiom();
    auto equal_err = evaluator.assume_equal(
      arena.apply(
        arena.argument(0),
        arena.argument(1)
      ),
      arena.apply(
        arena.copy(ax),
        arena.argument(1)
      )
    );
    REQUIRE(!equal_err);
    auto result_1 = evaluator.reduce(arena.apply(
      std::move(ax),
      arena.argument(1)
    ));
    auto result_2 = evaluator.reduce(arena.apply(
      arena.argument(0),
      arena.argument(1)
    ));
    REQUIRE(result_1 == result_2);
    result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
    REQUIRE(is_free_from_conglomerates(arena, result_1));
    result_1 = evaluator.reduce(std::move(result_1));
    REQUIRE(result_1 == result_2);
    arena.drop(std::move(result_1));
    arena.drop(std::move(result_2));
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can deal with pairs of equal declarations of nested fucntions.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    auto nest_expr = [&](std::size_t size) { //gives ($0 ($0 (... ($0 $1) ... ))) with "size" copies of $0
      auto ret = arena.argument(1);
      for(std::size_t i = 0; i < size; ++i) {
        ret = arena.apply(
          arena.argument(0),
          std::move(ret)
        );
      }
      return ret;
    };
    SECTION("The equality $0 $1 = $1 is accepted.") {
      EvaluationContext evaluator(arena, rules);
      auto equal_err = evaluator.assume_equal(nest_expr(1), nest_expr(0));
      REQUIRE(!equal_err);
      SECTION("The equality $0 $1 = $1 implies itself.") {
        auto result_1 = evaluator.reduce(nest_expr(1));
        auto result_2 = evaluator.reduce(nest_expr(0));
        REQUIRE(result_1 == result_2);
        result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
        REQUIRE(is_free_from_conglomerates(arena, result_1));
        result_1 = evaluator.reduce(std::move(result_1));
        REQUIRE(result_1 == result_2);
        arena.drop(std::move(result_1));
        arena.drop(std::move(result_2));
      }
      SECTION("The equality $0 $1 = $1 implies higher iterations are also $1") {
        EvaluationContext evaluator(arena, rules);
        auto equal_err = evaluator.assume_equal(nest_expr(1), nest_expr(0));
        for(std::size_t size = 2; size < 50; ++size) {
          auto result_1 = evaluator.reduce(nest_expr(size));
          auto result_2 = evaluator.reduce(nest_expr(0));
          REQUIRE(result_1 == result_2);
          result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
          REQUIRE(is_free_from_conglomerates(arena, result_1));
          result_1 = evaluator.reduce(std::move(result_1));
          REQUIRE(result_1 == result_2);
          arena.drop(std::move(result_1));
          arena.drop(std::move(result_2));
        }
      }
    }
    SECTION("Various single equalities of iterates of $0 applied to $1 work properly.") {
      for(std::size_t base = 0; base <= 5; ++base) {
        for(std::size_t step = 1; step <= 5; ++step) {
          EvaluationContext evaluator(arena, rules);
          auto equal_err = evaluator.assume_equal(nest_expr(base), nest_expr(base + step));
          REQUIRE(!equal_err);
          for(std::size_t repeats = 1; repeats <= 5; ++repeats) {
            auto result_1 = evaluator.reduce(nest_expr(base + step * repeats));
            auto result_2 = evaluator.reduce(nest_expr(base));
            REQUIRE(result_1 == result_2);
            result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
            REQUIRE(is_free_from_conglomerates(arena, result_1));
            result_1 = evaluator.reduce(std::move(result_1));
            REQUIRE(result_1 == result_2);
            arena.drop(std::move(result_1));
            arena.drop(std::move(result_2));
          }
        }
      }
    }
    SECTION("If $0 ($0 $1) = $1 and $0 ($0 ($0 $1)) = $1, then $0 $1 = $1") {
      EvaluationContext evaluator(arena, rules);
      auto equal_err = evaluator.assume_equal(nest_expr(2), nest_expr(0));
      auto equal_err2 = evaluator.assume_equal(nest_expr(3), nest_expr(0));
      REQUIRE(!equal_err);
      REQUIRE(!equal_err2);
      auto result_1 = evaluator.reduce(nest_expr(1));
      auto result_2 = evaluator.reduce(nest_expr(0));
      REQUIRE(result_1 == result_2);
      result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
      REQUIRE(is_free_from_conglomerates(arena, result_1));
      result_1 = evaluator.reduce(std::move(result_1));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("Various settings various pairs of iterates of $0 applied to $1 equal to $1 itself works properly.") {
      auto gcd = [](std::size_t x, std::size_t y) {
        while(x != 0) {
          y = y % x;
          std::swap(x, y);
        }
        return y;
      };
      for(std::size_t x = 1; x < 20; ++x) {
        for(std::size_t y = 1; y <= 20; ++y) {
          auto divisor = gcd(x, y);
          EvaluationContext evaluator(arena, rules);
          auto equal_err = evaluator.assume_equal(nest_expr(x), nest_expr(0));
          auto equal_err2 = evaluator.assume_equal(nest_expr(y), nest_expr(0));
          for(std::size_t z = 1; z < 50; ++z) {
            auto result_1 = evaluator.reduce(nest_expr(z));
            auto result_2 = evaluator.reduce(nest_expr(0));
            INFO("With iterates " << x << " and " << y << ", checking iterate " << z);
            if(z % divisor == 0) {
              REQUIRE(result_1 == result_2);
              result_1 = evaluator.eliminate_conglomerates(std::move(result_1));
              REQUIRE(is_free_from_conglomerates(arena, result_1));
              result_1 = evaluator.reduce(std::move(result_1));
              REQUIRE(result_1 == result_2);
            } else {
              REQUIRE(result_1 != result_2);
              auto free_1 = evaluator.eliminate_conglomerates(arena.copy(result_1));
              REQUIRE(is_free_from_conglomerates(arena, free_1));
              free_1 = evaluator.reduce(std::move(free_1));
              REQUIRE(free_1 == result_1);
              auto free_2 = evaluator.eliminate_conglomerates(arena.copy(result_2));
              REQUIRE(is_free_from_conglomerates(arena, free_2));
              free_2 = evaluator.reduce(std::move(free_2));
              REQUIRE(free_2 == result_2);
              destroy_from_arena(arena, free_1, free_2);
            }
            arena.drop(std::move(result_1));
            arena.drop(std::move(result_2));
          }
        }
      }
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext rejects (\\x.x) = $1.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto f = arena.declaration();
    rules.register_declaration(f);
    rules.add_rule(Rule{
      .pattern = {
        .head = arena.copy(f),
        .body = {
          .args_captured = 1
        }
      },
      .replacement = arena.argument(0)
    });

    auto equal_err = evaluator.assume_equal(
      std::move(f),
      arena.argument(1)
    );
    REQUIRE(equal_err);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext rejects (\\x.axiom) = $1.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto f = arena.declaration();
    rules.register_declaration(f);
    rules.add_rule(Rule{
      .pattern = {
        .head = arena.copy(f),
        .body = {
          .args_captured = 1
        }
      },
      .replacement = arena.axiom()
    });

    auto equal_err = evaluator.assume_equal(
      std::move(f),
      arena.argument(1)
    );
    REQUIRE(equal_err);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("EvaluationContext can assumptions regarding the double : Nat -> Nat function.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);
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
    SECTION("doubler is not lambda like") {
      REQUIRE(!evaluator.is_lambda_like(doubler));
    }
    SECTION("doubler zero evaluates to zero") { //double check for EvaluationContext
      auto result = evaluator.reduce(arena.apply(
        arena.copy(doubler),
        arena.copy(zero)
      ));
      REQUIRE(result == zero);
      destroy_from_arena(arena, result);
    }
    SECTION("doubler (doubler zero) evaluates to zero") {
      auto result = evaluator.reduce(arena.apply(
        arena.copy(doubler),
        arena.apply(
          arena.copy(doubler),
          arena.copy(zero)
        )
      ));
      REQUIRE(result == zero);
      destroy_from_arena(arena, result);
    }
    SECTION("doubler (succ zero) evaluates to succ (succ (zero))") {
      auto result = evaluator.reduce(arena.apply(
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
          arena.copy(zero)
        )
      );
      REQUIRE(result == expectation);
      destroy_from_arena(arena, result, expectation);
    }
    SECTION("doubler (succ (succ zero)) evaluates to succ (succ (succ (succ zero)))") {
      auto result = evaluator.reduce(arena.apply(
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
            arena.copy(succ),
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
    SECTION("The assumption doubler = $0 is rejected.") {
      auto equal_err = evaluator.assume_equal(
        arena.copy(doubler),
        arena.argument(0)
      );
      REQUIRE(equal_err);
    }
    SECTION("The assumption doubler $0 = $0 is accepted.") {
      auto equal_err = evaluator.assume_equal(
        arena.apply(
          arena.copy(doubler),
          arena.argument(0)
        ),
        arena.argument(0)
      );
      REQUIRE(!equal_err);
      auto result_1 = evaluator.reduce(arena.apply(
        arena.copy(doubler),
        arena.argument(0)
      ));
      auto result_2 = evaluator.reduce(arena.argument(0));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("The assumption doubler $0 = zero is accepted.") {
      auto equal_err = evaluator.assume_equal(
        arena.apply(
          arena.copy(doubler),
          arena.argument(0)
        ),
        arena.copy(zero)
      );
      REQUIRE(!equal_err);
      auto result_1 = evaluator.reduce(arena.apply(
        arena.copy(doubler),
        arena.argument(0)
      ));
      auto result_2 = evaluator.reduce(arena.copy(zero));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("The assumption doubler $0 = succ $1 is accepted.") {
      auto equal_err = evaluator.assume_equal(
        arena.apply(
          arena.copy(doubler),
          arena.argument(0)
        ),
        arena.apply(
          arena.copy(succ),
          arena.argument(1)
        )
      );
      REQUIRE(!equal_err);
      auto result_1 = evaluator.reduce(arena.apply(
        arena.copy(doubler),
        arena.argument(0)
      ));
      auto result_2 = evaluator.reduce(arena.apply(
        arena.copy(succ),
        arena.argument(1)
      ));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }
    SECTION("The assumption doubler $0 = succ zero is accepted.") {
      auto equal_err = evaluator.assume_equal(
        arena.apply(
          arena.copy(doubler),
          arena.argument(0)
        ),
        arena.apply(
          arena.copy(succ),
          arena.copy(zero)
        )
      );
      REQUIRE(!equal_err);
      auto result_1 = evaluator.reduce(arena.apply(
        arena.copy(doubler),
        arena.argument(0)
      ));
      auto result_2 = evaluator.reduce(arena.apply(
        arena.copy(succ),
        arena.copy(zero)
      ));
      REQUIRE(result_1 == result_2);
      arena.drop(std::move(result_1));
      arena.drop(std::move(result_2));
    }

    destroy_from_arena(arena, doubler, succ, zero);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Evaluation contexts can correctly identify lambda like expressions around add : Nat -> Nat -> Nat") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto add = arena.declaration();
    auto zero = arena.axiom();
    auto succ = arena.axiom();

    rules.register_declaration(add);
    rules.add_rule(Rule{ //add zero x = x
      .pattern = {
        .head = arena.copy(add),
        .body = {
          .args_captured = 2,
          .sub_matches = mdb::make_vector(PatternMatch{
            .substitution = arena.argument(0),
            .expected_head = arena.copy(zero),
            .args_captured = 0
          })
        }
      },
      .replacement = arena.argument(1)
    });
    rules.add_rule({ //add (succ x) y = succ (add x y)
      .pattern = {
        .head = arena.copy(add),
        .body = {
          .args_captured = 2,
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
          arena.copy(add),
          arena.argument(2),
          arena.argument(1)
        )
      )
    });

    SECTION("add is not considered lambda like") {
      REQUIRE(!evaluator.is_lambda_like(add));
    }
    SECTION("zero is not considered lambda like") {
      REQUIRE(!evaluator.is_lambda_like(zero));
    }
    SECTION("succ is not considered lambda like") {
      REQUIRE(!evaluator.is_lambda_like(succ));
    }
    SECTION("add zero is considered lambda like") {
      auto expr = arena.apply(
        arena.copy(add),
        arena.copy(zero)
      );
      REQUIRE(evaluator.is_lambda_like(expr));
      arena.drop(std::move(expr));
    }
    SECTION("add (succ zero) is considered lambda like") {
      auto expr = arena.apply(
        arena.copy(add),
        arena.apply(
          arena.copy(succ),
          arena.copy(zero)
        )
      );
      REQUIRE(evaluator.is_lambda_like(expr));
      arena.drop(std::move(expr));
    }
    destroy_from_arena(arena, add, succ, zero);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Evaluation contexts work properly with rules not consuming every argument") {
  Arena arena;
  {
    RuleCollector rules(arena);
    EvaluationContext evaluator(arena, rules);

    auto f = arena.declaration();
    auto g = arena.declaration();
    auto zero = arena.axiom();

    rules.register_declaration(f);
    rules.register_declaration(g);
    rules.add_rule(Rule{ //f = g
      .pattern = {
        .head = arena.copy(f),
        .body = {
          .args_captured = 0
        }
      },
      .replacement = arena.copy(g)
    });
    rules.add_rule({ //g x = x
      .pattern = {
        .head = arena.copy(g),
        .body = {
          .args_captured = 1
        }
      },
      .replacement = arena.argument(0)
    });

    auto r1 = evaluator.reduce(arena.copy(f));
    REQUIRE(r1 == g);
    auto r2 = evaluator.reduce(arena.apply(
      arena.copy(f),
      arena.copy(zero)
    ));
    REQUIRE(r2 == zero);

    destroy_from_arena(arena, f, g, zero, r1, r2);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
