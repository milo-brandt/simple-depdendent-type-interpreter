#include "../User/interactive_environment.hpp"
#include <catch.hpp>

//NOTE: Most tests of the whole integrated system are in full_case_impl.cpp and are auto-generated.
TEST_CASE("An interactive environment can be constructed and destroyed without polluting the arena.") {
  new_expression::Arena arena;
  {
    interactive::Environment environment(arena);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("An interactive environment can be constructed, compile something, and be destroyed without polluting the arena.") {
  new_expression::Arena arena;
  {
    interactive::Environment environment(arena);
    auto ret = environment.full_compile("Type").get_value();
    destroy_from_arena(arena, ret);
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
