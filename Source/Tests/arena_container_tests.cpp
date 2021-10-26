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
struct CountDestructor {
  int& x;
  void operator()(Arena&, int& entry) {
    x += entry;
  }
};
TEST_CASE("WeakKeyMap calls on_arena_destructor at appropriate times.") {
  int count_destroyed = 0;
  Arena arena;
  WeakKeyMap<int, CountDestructor> map(arena, CountDestructor{count_destroyed});
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(arena.copy(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;

  map.set(lhs_weak, 5);
  map.set(rhs_weak, 6);
  map.set(apply_weak, 7);

  REQUIRE(map.at(lhs_weak) == 5);
  REQUIRE(map.at(rhs_weak) == 6);
  REQUIRE(map.at(apply_weak) == 7);

  REQUIRE(count_destroyed == 0);

  arena.drop(std::move(apply));
  //it is not defined whether apply is still valid here - though it likely is.
  arena.clear_orphaned_expressions();
  REQUIRE(count_destroyed == 13); //we're holding a reference to lhs
  arena.drop(std::move(lhs));
  arena.clear_orphaned_expressions();
  REQUIRE(count_destroyed == 18); //we're holding a reference to lhs
  REQUIRE(arena.empty());
  //terms are now invalid; ensure the map erased them.
  REQUIRE(!map.contains(lhs_weak));
  REQUIRE(!map.contains(rhs_weak));
  REQUIRE(!map.contains(apply_weak));
}
TEST_CASE("OwnedKeySet models appropriate lifetime of elements.") {
  Arena arena;
  OwnedKeySet set(arena);
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(std::move(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;

  REQUIRE(set.insert(std::move(apply)));
  REQUIRE(set.contains(apply_weak));

  arena.clear_orphaned_expressions();

  REQUIRE(set.contains(apply_weak));
  //ensure that nothing was actually deleted.
  REQUIRE(arena.holds_apply(apply_weak));
  REQUIRE(arena.holds_axiom(lhs_weak));
  REQUIRE(arena.holds_axiom(rhs_weak));

  REQUIRE(set.erase(apply_weak));
  REQUIRE(!set.contains(apply_weak));

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("OwnedKeySet models appropriate lifetime of elements destroyed through its destructor.") {
  Arena arena;
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(std::move(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;

  {
    OwnedKeySet set(arena);

    REQUIRE(set.insert(std::move(apply)));
    REQUIRE(set.contains(apply_weak));

    arena.clear_orphaned_expressions();

    REQUIRE(set.contains(apply_weak));
    //ensure that nothing was actually deleted.
    REQUIRE(arena.holds_apply(apply_weak));
    REQUIRE(arena.holds_axiom(lhs_weak));
    REQUIRE(arena.holds_axiom(rhs_weak));
  }

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("OwnedKeyMap models appropriate lifetime of elements.") {
  Arena arena;
  OwnedKeyMap<std::uint64_t> map(arena);
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(std::move(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;
  map.set(std::move(apply), 17);
  REQUIRE(map.contains(apply_weak));
  REQUIRE(map.at(apply_weak) == 17);

  arena.clear_orphaned_expressions();

  REQUIRE(map.contains(apply_weak));
  REQUIRE(map.at(apply_weak) == 17);
  //ensure that nothing was actually deleted.
  REQUIRE(arena.holds_apply(apply_weak));
  REQUIRE(arena.holds_axiom(lhs_weak));
  REQUIRE(arena.holds_axiom(rhs_weak));

  map.erase(apply_weak);
  REQUIRE(!map.contains(apply_weak));

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("OwnedKeyMap models appropriate lifetime of elements destroyed through its destructor.") {
  Arena arena;
  auto lhs = arena.axiom();
  auto rhs = arena.axiom();
  auto apply = arena.apply(std::move(lhs), std::move(rhs));

  WeakExpression lhs_weak = lhs;
  WeakExpression rhs_weak = rhs;
  WeakExpression apply_weak = apply;

  {
    OwnedKeyMap<std::uint64_t> map(arena);

    map.set(std::move(apply), 17);
    REQUIRE(map.contains(apply_weak));
    REQUIRE(map.at(apply_weak) == 17);

    arena.clear_orphaned_expressions();

    REQUIRE(map.contains(apply_weak));
    REQUIRE(map.at(apply_weak) == 17);
    //ensure that nothing was actually deleted.
    REQUIRE(arena.holds_apply(apply_weak));
    REQUIRE(arena.holds_axiom(lhs_weak));
    REQUIRE(arena.holds_axiom(rhs_weak));
  }

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
