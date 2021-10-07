#include "../NewExpression/arena.hpp"
#include <catch.hpp>

using namespace new_expression;

namespace {
  struct U64Data : DataType {
    Arena& arena;
    std::uint64_t index;
    U64Data(Arena& arena, std::uint64_t index):arena(arena),index(index) {}
    void move_destroy(Buffer&, Buffer&) {}; //move second argument to first. Destroy second argument.
    void destroy(WeakExpression, Buffer&) {};
    void debug_print(Buffer const&, std::ostream&) {};
    void pretty_print(Buffer const&, std::ostream&, mdb::function<void(WeakExpression)>) {};
    bool all_subexpressions(mdb::function<bool(WeakExpression)>) { return true; };
    OwnedExpression modify_subexpressions(WeakExpression me, mdb::function<OwnedExpression(WeakExpression)>) { return arena.copy(me); };
  };
}
TEST_CASE("Arenas give equal indices to equal applications.") {
  Arena arena;
  auto a1 = arena.axiom();
  auto a2 = arena.axiom();
  REQUIRE(a1 != a2);
  auto apply_1 = arena.apply(arena.copy(a1), arena.copy(a2));
  auto apply_2 = arena.apply(std::move(a1), std::move(a2));
  REQUIRE(apply_1 == apply_2);
  arena.drop(std::move(apply_1));
  arena.drop(std::move(apply_2));

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Data in arenas can be recovered by getters.") {
  Arena arena;
  auto u64 = arena.create_data_type([](Arena& arena, std::uint64_t index) {
    return new U64Data{arena, index};
  });
  auto apply_lhs = arena.axiom();
  auto apply_rhs = arena.axiom();
  WeakExpression apply_lhs_weak = apply_lhs;
  WeakExpression apply_rhs_weak = apply_rhs;
  auto apply = arena.apply(std::move(apply_lhs), std::move(apply_rhs));
  auto data = arena.data(u64->index, [](Buffer* buffer){
    new (buffer) std::uint64_t{34};
  });
  /*
    This creates a table of tuples
    (name, example_value, get_if_..., get_..., holds_..., predicate)
    where name is a friendly name for the class, example_value is an OwnedExpression
    instance of the given class, the getters are member function pointers, and predicate
    tests whether the retrieved value equals the expected one.

    Then, using some template wizardry, we test every possible combination to ensure
    the right results.
  */
  auto cases = std::make_tuple(
    std::make_tuple("Apply", std::move(apply), &Arena::get_if_apply, &Arena::get_apply, &Arena::holds_apply, [&](Apply const& apply) { return apply.lhs == apply_lhs_weak && apply.rhs == apply_rhs_weak; }),
    std::make_tuple("Axiom", arena.axiom(), &Arena::get_if_axiom, &Arena::get_axiom, &Arena::holds_axiom,  [&](Axiom const& axiom) { return true; }),
    std::make_tuple("Declaration", arena.declaration(), &Arena::get_if_declaration, &Arena::get_declaration, &Arena::holds_declaration, [&](Declaration const& axiom) { return true; }),
    std::make_tuple("Data", std::move(data), &Arena::get_if_data, &Arena::get_data, &Arena::holds_data, [&](Data const& data) { return data.type_index == u64->index && (std::uint64_t&)data.buffer == 34; }),
    std::make_tuple("Argument", arena.argument(17), &Arena::get_if_argument, &Arena::get_argument, &Arena::holds_argument, [&](Argument const& argument) { return argument.index == 17; }),
    std::make_tuple("Conglomerate", arena.conglomerate(51), &Arena::get_if_conglomerate, &Arena::get_conglomerate, &Arena::holds_conglomerate, [&](Conglomerate const& conglomerate) { return conglomerate.index == 51; })
  );
  [&]<std::size_t... i>(std::index_sequence<i...>) {
    ([&] {
      constexpr auto outer_index = i;
      ([&] {
        constexpr auto inner_index = i;
        auto const& [name_val, val, ignore_1, ignore_2, ignore_3, ignore_4] = std::get<outer_index>(cases);
        auto const& [name_test, ignore_5, get_if, get, holds, eq_test] = std::get<inner_index>(cases);
        INFO("Checking value of type " << name_val << " against testers for " << name_test);
        if constexpr(outer_index == inner_index) {
          REQUIRE((arena.*get_if)(val) != nullptr);
          REQUIRE(eq_test(*(arena.*get_if)(val)));
          REQUIRE(eq_test((arena.*get)(val)));
          REQUIRE((arena.*holds)(val));
        } else {
          REQUIRE((arena.*get_if)(val) == nullptr);
          REQUIRE(!(arena.*holds)(val));
        }
      }() , ...);
    }() , ...);
  }(std::make_index_sequence<std::tuple_size_v<decltype(cases)> >{});
  std::apply([&](auto&... element) {
    (arena.drop(std::move(std::get<1>(element))) , ...); //drop the example values
  }, cases);

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
