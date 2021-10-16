#include "../Solver/solver.hpp"
#include "../NewExpression/type_utility.hpp"
#include "../NewExpression/arena_utility.hpp"
#include "../NewExpression/type_theory_primitives.hpp"
#include <catch.hpp>

using namespace new_expression;

TEST_CASE("The basic type theory primitives receive appropriate types.") {
  Arena arena;
  {
    RuleCollector rules(arena);
    TypeTheoryPrimitives primitives(arena, rules);
    EvaluationContext evaluator(arena, rules);
    WeakKeyMap<OwnedExpression, PartDestroyer> primitive_types(arena);
    for(auto& pair : primitives.get_types_of_primitives(arena)) {
      primitive_types.set(pair.first, std::move(pair.second));
    }
    auto nat = arena.axiom();
    primitive_types.set(nat, arena.copy(primitives.type));
    auto zero = arena.axiom();
    primitive_types.set(zero, arena.copy(nat));
    auto constant_type_over_nat = arena.apply(
      arena.copy(primitives.constant),
      arena.copy(primitives.type),
      arena.copy(primitives.type),
      arena.copy(nat)
    );
    auto succ = arena.axiom();
    primitive_types.set(succ, arena.apply(
      arena.copy(primitives.arrow),
      arena.copy(nat),
      arena.copy(constant_type_over_nat)
    ));

    TypeGetterContext context{
      .arrow = primitives.arrow,
      .type_of_primitive = [&](WeakExpression primitive) {
        return arena.copy(primitive_types.at(primitive));
      },
      .reduce_head = [&](OwnedExpression expr) {
        return evaluator.reduce(std::move(expr));
      }
    };
    SECTION("type has type type") {
      auto type_of_type = evaluator.reduce(
        get_type_of(arena, primitives.type, context)
      );
      REQUIRE(type_of_type == primitives.type);
      arena.drop(std::move(type_of_type));
    }
    SECTION("type_family nat has type type") {
      auto test_value = arena.apply(
        arena.copy(primitives.type_family),
        arena.copy(nat)
      );
      auto type_of = evaluator.reduce(
        get_type_of(arena, test_value, context)
      );
      REQUIRE(type_of == primitives.type);
      destroy_from_arena(arena, type_of, test_value);
    }
    SECTION("(arrow type type_family) has type type") {
      auto test_value = arena.apply(
        arena.copy(primitives.arrow),
        arena.copy(primitives.type),
        arena.copy(primitives.type_family)
      );
      auto type_of_type = evaluator.reduce(
        get_type_of(arena, test_value, context)
      );
      REQUIRE(type_of_type == primitives.type);
      destroy_from_arena(arena, type_of_type, test_value);
    }
    SECTION("(arrow type type_family) has type type") {
      auto test_value = arena.apply(
        arena.copy(primitives.arrow),
        arena.copy(primitives.type),
        arena.copy(primitives.type_family)
      );
      auto type_of_type = evaluator.reduce(
        get_type_of(arena, test_value, context)
      );
      REQUIRE(type_of_type == primitives.type);
      destroy_from_arena(arena, type_of_type, test_value);
    }
    SECTION("(constant nat zero type type) has type nat") {
      auto test_value = arena.apply(
        arena.copy(primitives.constant),
        arena.copy(nat),
        arena.copy(zero),
        arena.copy(primitives.type),
        arena.copy(primitives.type)
      );
      auto type_of = evaluator.reduce(
        get_type_of(arena, test_value, context)
      );
      REQUIRE(type_of == nat);
      destroy_from_arena(arena, type_of, test_value);
    }
    SECTION("constant_type_over_nat has type Nat -> Type") {
      auto type_of = evaluator.reduce(
        get_type_of(arena, constant_type_over_nat, context)
      );
      auto unfolded = unfold(arena, type_of);
      REQUIRE(unfolded.head == primitives.arrow);
      REQUIRE(unfolded.args.size() == 2);
      REQUIRE(unfolded.args[0] == nat);
      auto inner_application = evaluator.reduce(arena.apply(
        arena.copy(unfolded.args[1]),
        arena.axiom()
      ));
      REQUIRE(inner_application == primitives.type);
      destroy_from_arena(arena, type_of, inner_application);
    }
    SECTION("(arrow nat constant_type_over_nat) has type type") {
      auto test_value = arena.apply(
        arena.copy(primitives.arrow),
        arena.copy(nat),
        arena.copy(constant_type_over_nat)
      );
      auto type_of_type = evaluator.reduce(
        get_type_of(arena, test_value, context)
      );
      REQUIRE(type_of_type == primitives.type);
      destroy_from_arena(arena, type_of_type, test_value);
    }

    destroy_from_arena(arena, primitives, nat, zero, succ, constant_type_over_nat);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
