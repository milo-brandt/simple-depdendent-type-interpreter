#include "../Solver/manager.hpp"
#include <catch.hpp>


stack::Stack get_empty_stack_for(solver::BasicContext& context) {
  return stack::Stack::empty({
    .type = context.primitives.type,
    .arrow = context.primitives.arrow,
    .id = context.primitives.id,
    .constant = context.primitives.constant,
    .type_family = context.primitives.type_family,
    .arena = context.arena,
    .rule_collector = context.rule_collector,
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


TEST_CASE("var_1 = axiom is resolved by manager.") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    solver::Manager manager{context};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    manager.register_definable_indeterminate(arena.copy(var_1));
    auto axiom = arena.axiom();

    auto result = manager.register_equation({
      .lhs = arena.copy(var_1),
      .rhs = arena.copy(axiom),
      .stack = get_empty_stack_for(context)
    });
    REQUIRE(!result.is_ready());
    manager.run();
    REQUIRE(result.is_ready());
    REQUIRE(std::move(result).take() == solver::EquationResult::solved);
    REQUIRE(manager.solved());

    auto r1 = manager.reduce(arena.copy(var_1));
    REQUIRE(r1 == axiom);

    destroy_from_arena(arena, var_1, axiom, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("The stack test harness works.") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    auto stack = get_empty_stack_for(context);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("A simple cast is resolved by the manager.") {
  new_expression::Arena arena;
  {
    /*
      Setup a cast of
        axiom_1 : var_1
      to
        cast_var_1 : axiom_2
    */
    solver::BasicContext context{arena};
    solver::Manager manager{context};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    manager.register_definable_indeterminate(arena.copy(var_1));
    auto cast_var_1 = arena.declaration();
    context.rule_collector.register_declaration(cast_var_1);

    auto axiom_1 = arena.axiom();
    auto axiom_2 = arena.axiom();

    auto result = manager.register_cast({
      .stack = get_empty_stack_for(context),
      .variable = arena.copy(cast_var_1),
      .source_type = arena.copy(var_1),
      .source = arena.copy(axiom_1),
      .target_type = arena.copy(axiom_2)
    });
    REQUIRE(!result.is_ready());
    manager.run();
    REQUIRE(result.is_ready());
    REQUIRE(std::move(result).take() == solver::EquationResult::solved);
    REQUIRE(manager.solved());

    auto r1 = manager.reduce(arena.copy(var_1));
    REQUIRE(r1 == axiom_2);
    auto r2 = manager.reduce(arena.copy(cast_var_1));
    REQUIRE(r2 == axiom_1);

    destroy_from_arena(arena, var_1, cast_var_1, axiom_1, axiom_2, r1, r2);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("A simple function cast is resolved by the manager.") {
  new_expression::Arena arena;
  {
    /*
      Setup a cast to apply
        f : var_1 -> Type
        x : Type
      as
        cast_f cast_x
    */
    solver::BasicContext context{arena};
    solver::Manager manager{context};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    manager.register_definable_indeterminate(arena.copy(var_1));
    auto f = arena.axiom();
    auto x = arena.axiom();

    auto cast_f = arena.declaration();
    context.rule_collector.register_declaration(cast_f);
    auto cast_x = arena.declaration();
    context.rule_collector.register_declaration(cast_x);

    auto cast_domain = arena.declaration();
    context.rule_collector.register_declaration(cast_domain);
    manager.register_definable_indeterminate(arena.copy(cast_domain));
    auto cast_codomain = arena.declaration();
    context.rule_collector.register_declaration(cast_codomain);
    manager.register_definable_indeterminate(arena.copy(cast_codomain));

    auto result = manager.register_function_cast({
      .stack = get_empty_stack_for(context),
      .function_variable = arena.copy(cast_f),
      .argument_variable = arena.copy(cast_x),
      .function_value = arena.copy(f),
      .function_type = arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(var_1),
        arena.apply(
          arena.copy(context.primitives.constant),
          arena.copy(context.primitives.type),
          arena.copy(context.primitives.type),
          arena.copy(var_1)
        )
      ),
      .expected_function_type = arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(cast_domain),
        arena.copy(cast_codomain)
      ),
      .argument_value = arena.copy(x),
      .argument_type = arena.copy(context.primitives.type),
      .expected_argument_type = arena.copy(cast_domain)
    });

    REQUIRE(!result.is_ready());
    manager.run();
    REQUIRE(result.is_ready());
    REQUIRE(std::move(result).take() == solver::EquationResult::solved);
    REQUIRE(manager.solved());

    auto r1 = manager.reduce(arena.copy(var_1));
    REQUIRE(r1 == context.primitives.type);
    auto r2 = manager.reduce(arena.apply(
      arena.copy(cast_codomain), //should be \.Type
      arena.axiom()
    ));
    REQUIRE(r2 == context.primitives.type);
    auto r3 = manager.reduce(arena.copy(cast_domain));
    REQUIRE(r3 == context.primitives.type);
    auto r4 = manager.reduce(arena.copy(cast_f));
    REQUIRE(r4 == f);
    auto r5 = manager.reduce(arena.copy(cast_x));
    REQUIRE(r5 == x);

    destroy_from_arena(arena, var_1, f, x, cast_f, cast_x, cast_codomain, cast_domain, r1, r2, r3, r4, r5);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("A lambda function can be defined through the manager.") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    solver::Manager manager{context};

    auto nat = arena.axiom();
    auto zero = arena.axiom();
    auto f = arena.declaration();
    context.rule_collector.register_declaration(f);
    auto stack = get_empty_stack_for(context).extend(
      arena.copy(nat)
    ); //a context with one variable $0 : Nat.
    auto result = manager.register_rule({ //rule f $0 = $0
      .stack = stack,
      .pattern_type = arena.copy(nat),
      .pattern = arena.apply(
        arena.copy(f),
        arena.argument(0)
      ),
      .replacement_type = arena.copy(nat),
      .replacement = arena.argument(0)
    });
    REQUIRE(!result.is_ready());
    manager.run();
    REQUIRE(result.is_ready());
    REQUIRE(std::move(result).take());

    auto r1 = manager.reduce(arena.apply(
      arena.copy(f),
      arena.copy(zero)
    ));
    REQUIRE(r1 == zero);
    destroy_from_arena(arena, nat, zero, f, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
