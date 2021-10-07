#include "../NewExpression/on_arena_utility.hpp"
#include <vector>
#include <catch.hpp>


using namespace new_expression;

TEST_CASE("Part destroyer works on a vector of expressions.") {
  Arena arena;
  std::vector<OwnedExpression> expressions;
  expressions.push_back(arena.axiom());
  expressions.push_back(arena.axiom());
  expressions.push_back(arena.apply(arena.copy(expressions[0]), arena.copy(expressions[1])));

  destroy_from_arena(arena, expressions);
  //it is not defined whether apply is still valid here - though it likely is.
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
  //terms are now invalid; ensure the map erased them.
}
struct Complicated {
  OwnedExpression head;
  std::vector<std::pair<OwnedExpression, OwnedExpression> > pairs;
  static constexpr auto part_info = mdb::parts::simple<2>;
};
TEST_CASE("Part destroyer works recursively through a structure.") {
  Arena arena;
  Complicated item{
    .head = arena.axiom()
  };
  item.pairs.emplace_back(arena.declaration(), arena.axiom());
  item.pairs.emplace_back(arena.declaration(), arena.axiom());
  destroy_from_arena(arena, item);
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
