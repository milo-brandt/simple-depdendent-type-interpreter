#include "../NewExpression/arena_container.hpp"
#include <catch.hpp>

using namespace new_expression;

TEST_CASE("WeakKeyMap can recover referenced values, but forgets erased ones.") {
  Arena arena;
  WeakKeyMap<int> map(arena);
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(std::move(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;

  map.set(lhs_weak, 5);
  map.set(rhs_weak, 6);
  map.set(apply_weak, 7);

  REQUIRE(map.at(lhs_weak) == 5);
  REQUIRE(map.at(rhs_weak) == 6);
  REQUIRE(map.at(apply_weak) == 7);

  arena.drop(std::move(apply));
  //it is not defined whether apply is still valid here - though it likely is.
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
  //terms are now invalid; ensure the map erased them.
  REQUIRE(!map.contains(lhs_weak));
  REQUIRE(!map.contains(rhs_weak));
  REQUIRE(!map.contains(apply_weak));
}
TEST_CASE("WeakKeyMap can be safely moved.") {
  Arena arena;
  WeakKeyMap<int> map(arena);
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(std::move(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;

  map.set(lhs_weak, 5);
  map.set(rhs_weak, 6);
  map.set(apply_weak, 7);

  REQUIRE(map.at(lhs_weak) == 5);
  REQUIRE(map.at(rhs_weak) == 6);
  REQUIRE(map.at(apply_weak) == 7);

  WeakKeyMap<int> moved_map = std::move(map);

  REQUIRE(moved_map.at(lhs_weak) == 5);
  REQUIRE(moved_map.at(rhs_weak) == 6);
  REQUIRE(moved_map.at(apply_weak) == 7);

  arena.drop(std::move(apply));
  //it is not defined whether apply is still valid here - though it likely is.
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
  //terms are now invalid; ensure the map erased them.
  REQUIRE(!moved_map.contains(lhs_weak));
  REQUIRE(!moved_map.contains(rhs_weak));
  REQUIRE(!moved_map.contains(apply_weak));
}
