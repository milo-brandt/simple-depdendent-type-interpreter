#include "../Solver/manager.hpp"
#include "../Solver/evaluator.hpp"
#include "../NewExpression/arena_utility.hpp"
#include <catch.hpp>

template<std::size_t size>
auto make_embeder(new_expression::Arena& arena, new_expression::WeakExpression (&embeds)[size][2]) {
  return [&arena, &embeds](std::uint64_t index) {
    if(index < size) {
      return new_expression::TypedValue{
        arena.copy(embeds[index][0]),
        arena.copy(embeds[index][1])
      };
    } else {
      std::terminate();
    }
  };
}
template<class Embed>
solver::ExternalInterfaceParts interface_from_embed(Embed embed) {
  return solver::ExternalInterfaceParts{
    .explain_variable = [](auto&&...){},
    .embed = std::move(embed),
    .report_error = [](auto&&){}
  };
}
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
    auto simple_embed = make_embeder(arena, embeds);
    SECTION("Embedding a value gives the expected answer") {
      auto embed_index = (std::uint64_t)GENERATE(0, 1, 2);
      auto program_archive = archive([&]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {},
          .value = Embed{embed_index}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );
      REQUIRE(manager.solved());
      REQUIRE(ret.value == embeds[embed_index][0]);
      REQUIRE(ret.type == embeds[embed_index][1]);
      destroy_from_arena(arena, ret);
    }
    SECTION("Embedding primitives gives the expected values") {
      struct Case{
        compiler::new_instruction::Primitive primitive;
        new_expression::WeakExpression value;
        new_expression::WeakExpression type;
      };
      auto case_index = GENERATE(0, 1);
      Case cases[] = {
        {compiler::new_instruction::Primitive::type, context.primitives.type, context.primitives.type},
        {compiler::new_instruction::Primitive::arrow, context.primitives.arrow, context.primitives.arrow_type}
      };
      auto const& active_case = cases[case_index];
      auto program_archive = archive([&]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {},
          .value = PrimitiveExpression{active_case.primitive}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );
      REQUIRE(manager.solved());
      REQUIRE(ret.value == active_case.value);
      REQUIRE(ret.type == active_case.type);
      destroy_from_arena(arena, ret);
    }
    SECTION("A simple application is correctly interpreted") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
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
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
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
    SECTION("The TypeFamily element is correctly interpreted") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {},
          .value = TypeFamilyOver{
            Embed{2}
          }
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );
      REQUIRE(ret.type == context.primitives.type);
      ret.value = manager.reduce(std::move(ret.value));
      auto unfolded = unfold(arena, ret.value);
      REQUIRE(unfolded.head == context.primitives.arrow);
      REQUIRE(unfolded.args.size() == 2);
      REQUIRE(unfolded.args[0] == nat);
      auto inner_application = manager.reduce(arena.apply(
        arena.copy(unfolded.args[1]),
        arena.axiom()
      ));
      REQUIRE(inner_application == context.primitives.type);
      destroy_from_arena(arena, ret, inner_application);
    }
    SECTION("A declaration can be created") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {
            Declare{
              .type = Embed{2}
            }
          },
          .value = Local{0}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );

      REQUIRE(manager.solved());
      REQUIRE(arena.holds_declaration(ret.value));
      REQUIRE(ret.type == nat);
      destroy_from_arena(arena, ret);
    }
    SECTION("An axiom can be created") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {
            Axiom{
              .type = Embed{2}
            }
          },
          .value = Local{0}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );

      REQUIRE(manager.solved());
      REQUIRE(arena.holds_axiom(ret.value));
      REQUIRE(ret.type == nat);
      destroy_from_arena(arena, ret);
    }
    SECTION("An ill-typed application is not solved") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
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
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );
      REQUIRE(!manager.solved());
      manager.close();
      destroy_from_arena(arena, ret);
    }
    SECTION("A simple hole can be deduced.") {
      //Given f : (n : Nat) -> Nonneg n -> Nat
      //and x : T zero
      // can compile
      //block {
      //  let hole : Nat = _;
      //  f hole x
      //}
      auto zero = arena.axiom();
      auto nonneg = arena.axiom();
      auto f = arena.axiom();
      auto x = arena.axiom();
      auto nonneg_type = arena.apply(
        arena.copy(context.primitives.type_family),
        arena.copy(nat)
      );
      auto f_codomain = arena.declaration();
      context.rule_collector.register_declaration(f_codomain);
      context.rule_collector.add_rule({
        .pattern = lambda_pattern(arena.copy(f_codomain), 1),
        .replacement = arena.apply(
          arena.copy(context.primitives.arrow),
          arena.apply(
            arena.copy(nonneg),
            arena.argument(0)
          ),
          arena.apply(
            arena.copy(context.primitives.constant),
            arena.copy(context.primitives.type),
            arena.copy(nat),
            arena.apply(
              arena.copy(nonneg),
              arena.argument(0)
            )
          )
        )
      });
      auto f_type = arena.apply(
        arena.copy(context.primitives.arrow),
        arena.copy(nat),
        std::move(f_codomain)
      );
      auto x_type = arena.apply(
        arena.copy(nonneg),
        arena.copy(zero)
      );
      new_expression::RAIIDestroyer section_destroyer{arena,
        zero, nonneg, f, x, nonneg_type, f_type, x_type
      };
      new_expression::WeakExpression embeds[][2] = {
        {nat, context.primitives.type},
        {f, f_type},
        {x, x_type}
      };
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {
            DeclareHole{
              .type = Embed{0}
            }
          },
          .value = Apply{
            Apply{
              Embed{1},
              Local{0}
            },
            Embed{2}
          }
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          make_embeder(arena, embeds)
        ))
      );
      auto expected_value = arena.apply(
        arena.copy(f),
        arena.copy(zero),
        arena.copy(x)
      );
      ret.value = manager.reduce(std::move(ret.value));
      ret.type = manager.reduce(std::move(ret.type));

      REQUIRE(manager.solved());
      REQUIRE(ret.value == expected_value);
      REQUIRE(ret.type == nat);
      destroy_from_arena(arena, ret, expected_value);
    }
    SECTION("A simple type family defined by a lambda function is interpreted correctly.") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {
            Declare{ //codomain
              .type = TypeFamilyOver{
                Embed{2}
              }
            },
            Rule{
              .primary_pattern = PatternApply{
                PatternLocal{0},
                PatternCapture{0}
              },
              .replacement = Embed{2},
              .capture_count = 1
            }
          },
          .value = Local{0}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );

      REQUIRE(manager.solved());
      ret.value = manager.reduce(std::move(ret.value));
      ret.type = manager.reduce(std::move(ret.type));
      auto unfolded = unfold(arena, ret.type);
      REQUIRE(unfolded.head == context.primitives.arrow);
      REQUIRE(unfolded.args.size() == 2);
      REQUIRE(unfolded.args[0] == nat);
      auto application = manager.reduce(arena.apply(
        arena.copy(ret.value),
        arena.axiom()
      ));
      REQUIRE(application == nat);
      auto inner_application_type = manager.reduce(arena.apply(
        arena.copy(unfolded.args[1]),
        arena.axiom()
      ));
      REQUIRE(inner_application_type == context.primitives.type);
      destroy_from_arena(arena, ret, application, inner_application_type);
    }
    SECTION("The identity function Nat -> Nat can be defined through the evaluator.") {
      auto program_archive = archive([]() -> compiler::new_instruction::output::Program {
        using namespace compiler::new_instruction::output;
        return ProgramRoot{
          .commands = {
            Declare{ //codomain
              .type = TypeFamilyOver{
                Embed{2}
              }
            },
            Rule{
              .primary_pattern = PatternApply{
                PatternLocal{0},
                PatternCapture{0}
              },
              .replacement = Embed{2},
              .capture_count = 1
            },
            Declare{ //id
              .type = Apply{
                Apply{
                  PrimitiveExpression{compiler::new_instruction::Primitive::arrow},
                  Embed{2}
                },
                Local{0}
              }
            },
            Rule{
              .primary_pattern = PatternApply{
                PatternLocal{1},
                PatternCapture{0}
              },
              .replacement = Local{2},
              .capture_count = 1
            }
          },
          .value = Local{1}
        };
      }());
      auto ret = solver::evaluator::evaluate(
        program_archive.root().get_program_root(),
        manager.get_evaluator_interface(interface_from_embed(
          simple_embed
        ))
      );

      REQUIRE(manager.solved());

      ret.value = manager.reduce(std::move(ret.value));
      ret.type = manager.reduce(std::move(ret.type));
      auto unfolded = unfold(arena, ret.type);
      REQUIRE(unfolded.head == context.primitives.arrow);
      REQUIRE(unfolded.args.size() == 2);
      REQUIRE(unfolded.args[0] == nat);
      auto zero = arena.axiom();
      auto application = manager.reduce(arena.apply(
        arena.copy(ret.value),
        arena.copy(zero)
      ));
      REQUIRE(application == zero);
      auto inner_application_type = manager.reduce(arena.apply(
        arena.copy(unfolded.args[1]),
        arena.axiom()
      ));
      REQUIRE(inner_application_type == nat);
      destroy_from_arena(arena, ret, zero, application, inner_application_type);
    }
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
