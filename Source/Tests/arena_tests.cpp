#include "../NewExpression/arena.hpp"
#include <random>
#include <catch.hpp>

using namespace new_expression;

namespace {
  struct U64Data : DataType {
    OwnedExpression type;
    U64Data(Arena& arena, std::uint64_t index, OwnedExpression type):DataType(arena, index), type(std::move(type)) {}
    void destroy(WeakExpression, Buffer&) {};
    void debug_print(Buffer const&, std::ostream&) {};
    void pretty_print(Buffer const&, std::ostream&, mdb::function<void(WeakExpression)>) {};
    bool all_subexpressions(Buffer const&, mdb::function<bool(WeakExpression)>) { return true; };
    OwnedExpression modify_subexpressions(Buffer const&, WeakExpression me, mdb::function<OwnedExpression(WeakExpression)>) { return arena.copy(me); };
    OwnedExpression type_of(Buffer const&) { return arena.copy(type); }
    ~U64Data() { arena.drop(std::move(type)); }
  };
}
TEST_CASE("An arena that has done nothing is empty.") {
  Arena arena;
  REQUIRE(arena.empty());
}
TEST_CASE("An arena is empty after creating and dropping an axiom.") {
  Arena arena;
  auto a = arena.axiom();
  arena.drop(std::move(a));
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("An arena that has a living axiom is not empty.") {
  Arena arena;
  auto a = arena.axiom();
  REQUIRE(!arena.empty());
  arena.drop(std::move(a));
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("An arena that has a living axiom is not empty, even if its first allocation was destroyed.") {
  Arena arena;
  auto a = arena.axiom();
  auto b = arena.axiom();
  REQUIRE(!arena.empty());
  arena.drop(std::move(a));
  arena.clear_orphaned_expressions();
  REQUIRE(!arena.empty());
  arena.drop(std::move(b));
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("An arena is empty after creating and dropping an application of axioms.") {
  Arena arena;
  auto a1 = arena.axiom();
  auto a2 = arena.axiom();
  auto app = arena.apply(std::move(a1), std::move(a2));
  arena.drop(std::move(app));
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
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
TEST_CASE("Scalar C++ types embedded in arena are sensibly handled.") {
  Arena arena;
  auto u64_type = arena.axiom();
  auto u64 = arena.create_data_type([&](Arena& arena, std::uint64_t index) {
    return new U64Data{arena, index, arena.copy(u64_type)};
  });
  auto data = arena.data(u64->type_index, [](Buffer* buffer){
    new (buffer) std::uint64_t{51};
  });
  REQUIRE(arena.holds_data(data));
  REQUIRE(arena.get_data(data).type_index == u64->type_index);
  REQUIRE((std::uint64_t const&)arena.get_data(data).buffer == 51);
  REQUIRE(arena.all_subexpressions_of(arena.get_data(data), [&](WeakExpression) -> bool {
    std::terminate(); //this shouldn't be called
  }));
  auto data_modified = arena.modify_subexpressions_of(arena.get_data(data), [&](WeakExpression) -> OwnedExpression {
    std::terminate(); //this shouldn't be called
  });
  REQUIRE(data_modified == data);

  arena.drop(std::move(data));
  arena.drop(std::move(data_modified));
  arena.drop(std::move(u64_type));

  u64 = nullptr; //release reference
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Scalar C++ types are not destroyed early.") {
  Arena arena;
  auto u64 = arena.create_data_type([](Arena& arena, std::uint64_t index) {
    return new U64Data{arena, index, arena.axiom()};
  });
  auto data = arena.data(u64->type_index, [](Buffer* buffer){
    new (buffer) std::uint64_t{51};
  });
  REQUIRE(arena.holds_data(data));
  REQUIRE(arena.get_data(data).type_index == u64->type_index);
  REQUIRE((std::uint64_t const&)arena.get_data(data).buffer == 51);

  u64 = nullptr; //release reference before last element referencing it is destroyed
  arena.clear_orphaned_expressions();

  REQUIRE(arena.holds_data(data));

  arena.drop(std::move(data));

  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Data in arenas can be recovered by getters.") {
  Arena arena;
  auto u64 = arena.create_data_type([](Arena& arena, std::uint64_t index) {
    return new U64Data{arena, index, arena.axiom()};
  });
  auto apply_lhs = arena.axiom();
  auto apply_rhs = arena.axiom();
  WeakExpression apply_lhs_weak = apply_lhs;
  WeakExpression apply_rhs_weak = apply_rhs;
  auto apply = arena.apply(std::move(apply_lhs), std::move(apply_rhs));
  auto data = arena.data(u64->type_index, [](Buffer* buffer){
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
    std::make_tuple("Data", std::move(data), &Arena::get_if_data, &Arena::get_data, &Arena::holds_data, [&](Data const& data) { return data.type_index == u64->type_index && (std::uint64_t const&)data.buffer == 34; }),
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

  u64 = nullptr; //release reference
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
TEST_CASE("Arena allocation survives fuzzing.") {
  std::mt19937 random_gen{17}; //deterministic random number generator
  std::uniform_int_distribution index_distribution{0, 15};
  std::uniform_int_distribution maybe_index_distribution{0, 18};
  Arena arena;
  OwnedExpression expressions[16] = {
    arena.axiom(), arena.axiom(), arena.axiom(), arena.axiom(),
    arena.axiom(), arena.axiom(), arena.axiom(), arena.axiom(),
    arena.axiom(), arena.axiom(), arena.axiom(), arena.axiom(),
    arena.axiom(), arena.axiom(), arena.axiom(), arena.axiom()
  };
  auto get_index = [&](std::size_t i) {
    if(i >= 16) return arena.axiom();
    else return arena.copy(expressions[i]);
  };
  for(std::size_t i = 0; i < 100000; ++i) { //randomy choose indices i, j, and k and update expr[k] = apply(expr[i], expr[j]).
    //With probability 1/17, puts i and/or j out of range, in which case a new axiom is used in place of expr[i].
    auto lhs = maybe_index_distribution(random_gen);
    auto rhs = maybe_index_distribution(random_gen);
    auto target = index_distribution(random_gen);
    auto new_value = arena.apply(get_index(lhs), get_index(rhs));
    arena.drop(std::move(expressions[target]));
    expressions[target] = std::move(new_value);
  }
  for(auto& expr : expressions) {
    arena.drop(std::move(expr));
  }
  arena.clear_orphaned_expressions();
  REQUIRE(arena.empty());
}
