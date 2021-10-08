#include "../NewExpression/arena_utility.hpp"
#include <catch.hpp>


using namespace new_expression;

TEST_CASE("Expressions are correctly destructured.") {
  Arena arena;
  auto head = arena.declaration();
  auto arg_0 = arena.axiom();
  auto arg_1 = arena.axiom();
  auto arg_2 = arena.axiom();

  WeakExpression head_weak = head;
  WeakExpression arg_0_weak = arg_0;
  WeakExpression arg_1_weak = arg_1;
  WeakExpression arg_2_weak = arg_2;

  auto big = arena.apply(arena.apply(arena.apply(std::move(head), std::move(arg_0)), std::move(arg_1)), std::move(arg_2));

  SECTION("unfold correctly unfolds expressions") {
    auto unfolded = unfold(arena, big);
    REQUIRE(unfolded.head == head_weak);
    REQUIRE(unfolded.args.size() == 3);
    REQUIRE(unfolded.args[0] == arg_0_weak);
    REQUIRE(unfolded.args[1] == arg_1_weak);
    REQUIRE(unfolded.args[2] == arg_2_weak);

    arena.drop(std::move(big));
  }
  SECTION("unfold_owned correctly unfolds expressions") {
    auto unfolded = unfold_owned(arena, std::move(big));
    REQUIRE(unfolded.head == head_weak);
    REQUIRE(unfolded.args.size() == 3);
    REQUIRE(unfolded.args[0] == arg_0_weak);
    REQUIRE(unfolded.args[1] == arg_1_weak);
    REQUIRE(unfolded.args[2] == arg_2_weak);
    arena.drop(std::move(unfolded.head));
    for(auto& arg : unfolded.args) arena.drop(std::move(arg));
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Expressions can be substituted into.") {
  Arena arena;
  OwnedExpression args[3] = {
    arena.axiom(),
    arena.declaration(),
    arena.axiom()
  };
  SECTION("Substitution into an axiom does nothing") {
    auto new_expr = substitute_into(arena, args[0], args);
    REQUIRE(new_expr == args[0]);
    arena.drop(std::move(new_expr));
  }
  SECTION("Substitution into a declaration does nothing") {
    auto new_expr = substitute_into(arena, args[1], args);
    REQUIRE(new_expr == args[1]);
    arena.drop(std::move(new_expr));
  }
  SECTION("Substitution into an argument returns the corresponding arg.") {
    for(std::size_t i = 0; i < 3; ++i) {
      auto substitution = arena.argument(i);
      auto new_expr = substitute_into(arena, substitution, args);
      REQUIRE(new_expr == args[i]);
      arena.drop(std::move(substitution));
      arena.drop(std::move(new_expr));
    }
  }
  SECTION("Substitution into an application gives appropriate value.") {
    auto substitution = arena.apply(arena.argument(0), arena.argument(1));
    auto new_expr = substitute_into(arena, substitution, args);
    auto manual = arena.apply(arena.copy(args[0]), arena.copy(args[1]));
    REQUIRE(new_expr == manual);
    arena.drop(std::move(substitution));
    arena.drop(std::move(new_expr));
    arena.drop(std::move(manual));
  }
  SECTION("Substitution into a deep application gives appropriate value.") {
    auto substitution = arena.apply(
      arena.copy(args[2]),
      arena.apply(
        arena.copy(args[2]),
        arena.argument(0)
      )
    );
    auto new_expr = substitute_into(arena, substitution, args);
    auto manual = arena.apply(
      arena.copy(args[2]),
      arena.apply(
        arena.copy(args[2]),
        arena.copy(args[0])
      )
    );
    REQUIRE(new_expr == manual);
    arena.drop(std::move(substitution));
    arena.drop(std::move(new_expr));
    arena.drop(std::move(manual));
  }

  for(auto& arg : args) arena.drop(std::move(arg));
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
