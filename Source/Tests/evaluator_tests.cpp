#include "../Solver/manager.hpp"
#include "../Solver/evaluator.hpp"
#include <catch.hpp>

TEST_CASE("The evaluator can handle simple programs") {
  new_expression::Arena arena;
  {
    solver::BasicContext context{arena};
    solver::Manager manager{context};
    auto nat = arena.axiom();
    new_expression::RAIIDestroyer nat_destroyer{arena, nat};
    new_expression::WeakExpression embeds[][2] = {
      {context.primitives.type, context.primitives.type},
      {context.primitives.arrow, context.primitives.arrow_type},
      {nat, context.primitives.type}
    };
    auto simple_embed = [&](std::uint64_t index) {
      if(index < std::extent_v<decltype(embeds)>) {
        return new_expression::TypedValue{
          arena.copy(embeds[index][0]),
          arena.copy(embeds[index][1])
        };
      } else {
        std::terminate();
      }
    };
    SECTION("Embedding a value gives the expected answer") {
      auto program_archive = archive([]() -> compiler::instruction::output::Program {
        using namespace compiler::instruction::output;
        return ProgramRoot{
          .commands = {},
          .value = Embed{2}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(simple_embed)
      );
      REQUIRE(manager.solved());
      REQUIRE(ret.value == nat);
      REQUIRE(ret.type == context.primitives.type);
      destroy_from_arena(arena, ret);
    }
    SECTION("A simple application is correctly interpreted") {
      auto program_archive = archive([]() -> compiler::instruction::output::Program {
        using namespace compiler::instruction::output;
        return ProgramRoot{
          .commands = {},
          .value = Apply{
            Embed{1},
            Embed{2}
          }
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(simple_embed)
      );
      auto expected_value = arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(nat)
      );
      auto expected_type = arena.apply(
        arena.copy(context.primitives.arrow_codomain),
        arena.copy(nat)
      );

      //NOTE: this test may fail if the evaluator introduces indeterminates to
      //try to cast things around. It is not allowed to do this because creating
      //variables for *every* application makes it hard to issue good diagnostics.
      //
      //We do reduce the types, however, since they can reduce and the evaluator
      //is permitted to reduce or not reduce as it likes.
      ret.type = manager.reduce(std::move(ret.type));
      expected_type = manager.reduce(std::move(expected_type));

      REQUIRE(manager.solved());
      REQUIRE(ret.value == expected_value);
      REQUIRE(ret.type == expected_type);
      destroy_from_arena(arena, ret, expected_value, expected_type);
    }
    SECTION("An ill-typed application is not solved") {
      auto program_archive = archive([]() -> compiler::instruction::output::Program {
        using namespace compiler::instruction::output;
        return ProgramRoot{
          .commands = {},
          .value = Apply{
            Embed{0},
            Embed{0}
          }
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(simple_embed)
      );
      REQUIRE(!manager.solved());
      manager.close();
      destroy_from_arena(arena, ret);
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
