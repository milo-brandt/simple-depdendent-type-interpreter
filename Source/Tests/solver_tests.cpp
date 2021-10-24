#include "../Solver/solver.hpp"
#include "../NewExpression/arena_utility.hpp"
#include <catch.hpp>

stack::Stack get_empty_stack_for(new_expression::Arena& arena, new_expression::RuleCollector& rule_collector) {
  auto placeholder = arena.axiom();
  arena.drop(std::move(placeholder));
  return stack::Stack::empty({
    .type = placeholder, //ugly hack
    .arrow = placeholder,
    .id = placeholder,
    .constant = placeholder,
    .type_family = placeholder,
    .arena = arena,
    .rule_collector = rule_collector,
    .type_collector = *(new_expression::PrimitiveTypeCollector*)nullptr,
    .register_type = [&arena](new_expression::WeakExpression, new_expression::OwnedExpression t) {
      arena.drop(std::move(t));
    },
    .register_declaration = [&rule_collector](new_expression::WeakExpression expr) {
      rule_collector.register_declaration(expr);
    },
    .add_rule = [&rule_collector](new_expression::Rule rule) {
      rule_collector.add_rule(std::move(rule));
    }
  });
}

struct SimpleContext {
  new_expression::Arena& arena;
  new_expression::RuleCollector rule_collector;
  new_expression::EvaluationContext evaluation_context;
  std::unordered_set<void const*> indeterminate_indices;
  SimpleContext(new_expression::Arena& arena):
    arena(arena),
    rule_collector(arena),
    evaluation_context(arena, rule_collector) {}
  solver::SolverInterface interface() {
    return {
      .arena = arena,
      .term_depends_on = [this](new_expression::WeakExpression lhs, new_expression::WeakExpression rhs) {
        return lhs == rhs;
      },
      .get_definable_info = [this](new_expression::WeakExpression expr) -> std::optional<solver::DefinableInfo> {
        if(indeterminate_indices.contains(expr.data())) {
          return solver::DefinableInfo{
            .stack = get_empty_stack_for(arena, rule_collector)
          };
        } else {
          return std::nullopt;
        }
      },
      .is_lambda_like = [this](new_expression::WeakExpression expr) {
        return evaluation_context.is_lambda_like(expr);
      },
      .is_head_closed = [this](new_expression::WeakExpression expr) {
        auto head = unfold(arena, expr).head;
        return arena.holds_axiom(head) || arena.holds_argument(head);
        //note: arguments might not be entirely closed with assumptions - should check in real code
      },
      .make_definition = [this](solver::IndeterminateDefinition definition) {
        if(!indeterminate_indices.contains(definition.head.data()))
          std::terminate();
        indeterminate_indices.erase(definition.head.data());
        rule_collector.add_rule({
          .pattern = lambda_pattern(arena.copy(definition.head), definition.arg_count),
          .replacement = std::move(definition.replacement)
        });
      }
    };
  }
};

using namespace solver;

TEST_CASE("var_1 = axiom is resolved") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.copy(var_1),
        .rhs = arena.copy(axiom),
        .stack = get_empty_stack_for(arena, context.rule_collector)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());

    auto r1 = context.evaluation_context.reduce(arena.copy(var_1));
    REQUIRE(r1 == axiom);

    destroy_from_arena(arena, var_1, axiom, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("var_1 = declaration is resolved") {
  //this is mostly to check .term_depends_on behavior
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto declaration = arena.declaration();
    context.rule_collector.register_declaration(declaration);
    Solver solver{
      context.interface(),
      {
        .lhs = arena.copy(var_1),
        .rhs = arena.copy(declaration),
        .stack = get_empty_stack_for(arena, context.rule_collector)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());

    auto r1 = context.evaluation_context.reduce(arena.copy(var_1));
    REQUIRE(r1 == declaration);

    destroy_from_arena(arena, var_1, declaration, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}

TEST_CASE("axiom = var_1 is resolved") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.copy(axiom),
        .rhs = arena.copy(var_1),
        .stack = get_empty_stack_for(arena, context.rule_collector)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());

    auto r1 = context.evaluation_context.reduce(arena.copy(var_1));
    REQUIRE(r1 == axiom);

    destroy_from_arena(arena, var_1, axiom, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}

TEST_CASE("var_1 $0 = axiom is resolved") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(var_1),
          arena.argument(0)
        ),
        .rhs = arena.copy(axiom),
        .stack = get_empty_stack_for(arena, context.rule_collector).extend(arena.axiom())
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());

    auto r1 = context.evaluation_context.reduce(arena.apply(
      arena.copy(var_1),
      arena.argument(17) //rule should apply to absolutely anything
    ));
    REQUIRE(r1 == axiom);

    destroy_from_arena(arena, var_1, axiom, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("var_1 $0 = var_2 $1 stalls") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto var_2 = arena.declaration();
    context.rule_collector.register_declaration(var_2);
    context.indeterminate_indices.insert(var_2.data());
    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(var_1),
          arena.argument(0)
        ),
        .rhs = arena.apply(
          arena.copy(var_2),
          arena.argument(1)
        ),
        .stack = get_empty_stack_for(arena, context.rule_collector).extend(arena.axiom()).extend(arena.axiom())
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(!solver.solved());
    REQUIRE(!solver.failed());

    destroy_from_arena(arena, var_1, var_2);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}

TEST_CASE("var_1 $0 = var_2 $1 and var_2 $0 = axiom succeeds") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto var_2 = arena.declaration();
    context.rule_collector.register_declaration(var_2);
    context.indeterminate_indices.insert(var_2.data());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(var_1),
          arena.argument(0)
        ),
        .rhs = arena.apply(
          arena.copy(var_2),
          arena.argument(1)
        ),
        .stack = get_empty_stack_for(arena, context.rule_collector).extend(arena.axiom()).extend(arena.axiom())
      }
    };
    Solver solver_2{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(var_2),
          arena.argument(0)
        ),
        .rhs = arena.copy(axiom),
        .stack = get_empty_stack_for(arena, context.rule_collector).extend(arena.axiom())
      }
    };
    while(solver.try_to_make_progress() || solver_2.try_to_make_progress());
    REQUIRE(solver.solved());
    REQUIRE(solver_2.solved());

    destroy_from_arena(arena, var_1, var_2, axiom);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("f = g where f $0 = $0 and g $0 = $0 succeeds.") {
  //this tests deepening
  new_expression::Arena arena;
  {
    SimpleContext context{arena};

    auto f = arena.declaration();
    context.rule_collector.register_declaration(f);
    context.rule_collector.add_rule({ //f x = x
      .pattern = lambda_pattern(arena.copy(f), 1),
      .replacement = arena.argument(0)
    });
    auto g = arena.declaration();
    context.rule_collector.register_declaration(g);
    context.rule_collector.add_rule({ //f x = x
      .pattern = lambda_pattern(arena.copy(g), 1),
      .replacement = arena.argument(0)
    });
    Solver solver{
      context.interface(),
      {
        .lhs = arena.copy(f),
        .rhs = arena.copy(g),
        .stack = get_empty_stack_for(arena, context.rule_collector)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());

    destroy_from_arena(arena, f, g);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("$0 = $1 fails.") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};

    Solver solver{
      context.interface(),
      {
        .lhs = arena.argument(0),
        .rhs = arena.argument(1),
        .stack = get_empty_stack_for(arena, context.rule_collector).extend(arena.axiom()).extend(arena.axiom())
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.failed());
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("axiom_1 = axiom_2 fails.") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};

    Solver solver{
      context.interface(),
      {
        .lhs = arena.axiom(),
        .rhs = arena.axiom(),
        .stack = get_empty_stack_for(arena, context.rule_collector).extend(arena.axiom()).extend(arena.axiom())
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.failed());
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("axiom_1 var_1 = axiom_1 axiom_2 succeeds.") {
  //this tests symmetric explosion
  new_expression::Arena arena;
  {
    SimpleContext context{arena};

    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto axiom_1 = arena.axiom();
    auto axiom_2 = arena.axiom();

    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(axiom_1),
          arena.copy(var_1)
        ),
        .rhs = arena.apply(
          arena.copy(axiom_1),
          arena.copy(axiom_2)
        ),
        .stack = get_empty_stack_for(arena, context.rule_collector)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());
    auto r1 = context.evaluation_context.reduce(arena.copy(var_1));
    REQUIRE(r1 == axiom_2);
    destroy_from_arena(arena, var_1, axiom_1, axiom_2, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("f var_1 = f var_1 succeeds even for undefined declaration f.") {
  //this tests judgemental equality
  new_expression::Arena arena;
  {
    SimpleContext context{arena};

    auto f = arena.declaration();
    context.rule_collector.register_declaration(f);
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());

    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(f),
          arena.copy(var_1)
        ),
        .rhs = arena.apply(
          arena.copy(f),
          arena.copy(var_1)
        ),
        .stack = get_empty_stack_for(arena, context.rule_collector)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());
    destroy_from_arena(arena, f, var_1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("axiom_1 var_1 = $0 succeeds in a stack where $0 = axiom_1 axiom_2.") {
  //this tests symmetric explosion
  new_expression::Arena arena;
  {
    SimpleContext context{arena};

    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.data());
    auto axiom_1 = arena.axiom();
    auto axiom_2 = arena.axiom();
    auto dummy_type = arena.axiom();

    auto stack = get_empty_stack_for(arena, context.rule_collector)
      .extend(arena.copy(dummy_type))
      .extend_by_assumption(new_expression::TypedValue{
        .value = arena.argument(0),
        .type = arena.copy(dummy_type)
      }, new_expression::TypedValue{
        .value = arena.apply(
          arena.copy(axiom_1),
          arena.copy(axiom_2)
        ),
        .type = arena.copy(dummy_type)
      });

    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(axiom_1),
          arena.copy(var_1)
        ),
        .rhs = arena.argument(0),
        .stack = std::move(stack)
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.solved());
    auto r1 = context.evaluation_context.reduce(arena.copy(var_1));
    REQUIRE(r1 == axiom_2);
    destroy_from_arena(arena, var_1, axiom_1, axiom_2, dummy_type, r1);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
