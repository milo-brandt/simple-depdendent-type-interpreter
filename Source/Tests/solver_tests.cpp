#include "../Solver/solver.hpp"
#include "../NewExpression/arena_utility.hpp"
#include <catch.hpp>

struct SimpleContext {
  new_expression::Arena& arena;
  new_expression::RuleCollector rule_collector;
  new_expression::EvaluationContext evaluation_context;
  std::unordered_set<std::uint64_t> indeterminate_indices;
  SimpleContext(new_expression::Arena& arena):
    arena(arena),
    rule_collector(arena),
    evaluation_context(arena, rule_collector) {}
  solver::SolverInterface interface() {
    return {
      .arena = arena,
      .reduce = [this](new_expression::OwnedExpression expr) {
        return evaluation_context.reduce(std::move(expr));
      },
      .term_depends_on = [this](new_expression::WeakExpression lhs, new_expression::WeakExpression rhs) {
        return lhs != rhs;
      },
      .is_definable_indeterminate = [this](new_expression::WeakExpression expr) {
        return indeterminate_indices.contains(expr.index());
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
        if(!indeterminate_indices.contains(definition.head.index()))
          std::terminate();
        indeterminate_indices.erase(definition.head.index());
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
    context.indeterminate_indices.insert(var_1.index());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.copy(var_1),
        .rhs = arena.copy(axiom),
        .depth = 0
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
TEST_CASE("axiom = var_1 is resolved") {
  new_expression::Arena arena;
  {
    SimpleContext context{arena};
    auto var_1 = arena.declaration();
    context.rule_collector.register_declaration(var_1);
    context.indeterminate_indices.insert(var_1.index());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.copy(axiom),
        .rhs = arena.copy(var_1),
        .depth = 0
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
    context.indeterminate_indices.insert(var_1.index());
    auto axiom = arena.axiom();
    Solver solver{
      context.interface(),
      {
        .lhs = arena.apply(
          arena.copy(var_1),
          arena.argument(0)
        ),
        .rhs = arena.copy(axiom),
        .depth = 1
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
    context.indeterminate_indices.insert(var_1.index());
    auto var_2 = arena.declaration();
    context.rule_collector.register_declaration(var_2);
    context.indeterminate_indices.insert(var_2.index());
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
        .depth = 2
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
    context.indeterminate_indices.insert(var_1.index());
    auto var_2 = arena.declaration();
    context.rule_collector.register_declaration(var_2);
    context.indeterminate_indices.insert(var_2.index());
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
        .depth = 2
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
        .depth = 1
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
        .depth = 0
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
        .depth = 2
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
        .depth = 2
      }
    };
    while(solver.try_to_make_progress());
    REQUIRE(solver.failed());
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
