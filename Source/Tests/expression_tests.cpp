#include <catch.hpp>
#include "../expressions.hpp"

using namespace expressionz;

TEST_CASE("expressions can be constructed with primitive types") {
  expression<int> x = argument(0); //just check for compilation
  auto result = x.local_form();
  REQUIRE(holds_alternative<local_type::argument>(result));
  REQUIRE(get<local_type::argument>(result).index == 0);

  expression y = 5; //checks deduction
  auto result2 = y.local_form();
  REQUIRE(holds_alternative<local_type::value>(result2));
  REQUIRE(get<local_type::value>(result2) == 5);

  expression z = apply(7, 13);
  auto result3 = z.local_form();
  REQUIRE(holds_alternative<local_type::apply>(result3));
}
TEST_CASE("deepen works") {
  expression x = deepen(1, argument(0));
  auto result = x.local_form();
  REQUIRE(holds_alternative<local_type::argument>(result));
  REQUIRE(get<local_type::argument>(result).index == 1);

  expression y = deepen(1, 1, argument(0));
  result = y.local_form();
  REQUIRE(holds_alternative<local_type::argument>(result));
  REQUIRE(get<local_type::argument>(result).index == 0);
}
TEST_CASE("substitute works") {
  SECTION("substitutes for 0th argument") {
    expression x = substitute(5, argument(0));
    auto result = x.local_form();
    REQUIRE(holds_alternative<local_type::value>(result));
    REQUIRE(get<local_type::value>(result) == 5);
  }
  SECTION("decreases higher indices") {
    expression x = substitute(5, argument(1));
    auto result = x.local_form();
    REQUIRE(holds_alternative<local_type::argument>(result));
    REQUIRE(get<local_type::argument>(result).index == 0);
  }
  SECTION("preserves lower indices") {
    expression x = substitute(1, 5, argument(0));
    auto result = x.local_form();
    REQUIRE(holds_alternative<local_type::argument>(result));
    REQUIRE(get<local_type::argument>(result).index == 0);
  }
}
TEST_CASE("bind works") {
  auto map = [](int x) -> expression<double> {
    if(x == 0) return argument(0);
    else return 17.289;
  };
  expression x = bind(map, (expression<int>)1);
  auto result = x.local_form();
  REQUIRE(holds_alternative<local_type::value>(result));
  REQUIRE(get<local_type::value>(result) == 17.289);

  x = bind(map, 0);
  result = x.local_form();
  REQUIRE(holds_alternative<local_type::argument>(result));
  REQUIRE(get<local_type::argument>(result).index == 0);
}
TEST_CASE("normal form works") {
  expression<int> doubler = abstract(expression<int>(abstract(apply(argument(1), apply(argument(1), argument(0))))));
  expression<int> constant_17 = abstract(17);
  expression<int> id_f = abstract(argument(0));
  SECTION("applying constant function works") {
    expression<int> test = apply(constant_17, 5);
    auto result = test.normal_form();
    REQUIRE(holds_alternative<normal_type::apply_to_primitive>(result));
    REQUIRE(get<normal_type::apply_to_primitive>(result).head == 17);
    REQUIRE(get<normal_type::apply_to_primitive>(result).args.size() == 0);
  }
  SECTION("applying doubled constant function works") {
    expression<int> test = apply(apply(doubler, constant_17), 5);
    auto result = test.normal_form();
    REQUIRE(holds_alternative<normal_type::apply_to_primitive>(result));
    REQUIRE(get<normal_type::apply_to_primitive>(result).head == 17);
    REQUIRE(get<normal_type::apply_to_primitive>(result).args.size() == 0);
  }
  SECTION("applying doubled doubled constant function works") {
    expression<int> test = apply(apply(apply(doubler, doubler), constant_17), 5);
    auto result = test.normal_form();
    REQUIRE(holds_alternative<normal_type::apply_to_primitive>(result));
    REQUIRE(get<normal_type::apply_to_primitive>(result).head == 17);
    REQUIRE(get<normal_type::apply_to_primitive>(result).args.size() == 0);
  }
  SECTION("applying identity function works") {
    expression<int> test = apply(id_f, 17);
    auto result = test.normal_form();
    REQUIRE(holds_alternative<normal_type::apply_to_primitive>(result));
    REQUIRE(get<normal_type::apply_to_primitive>(result).head == 17);
    REQUIRE(get<normal_type::apply_to_primitive>(result).args.size() == 0);
  }
  SECTION("applying doubled identity function works") {
    expression<int> test = apply(apply(doubler, id_f), 17);
    auto result = test.normal_form();
    REQUIRE(holds_alternative<normal_type::apply_to_primitive>(result));
    REQUIRE(get<normal_type::apply_to_primitive>(result).head == 17);
    REQUIRE(get<normal_type::apply_to_primitive>(result).args.size() == 0);
  }
  SECTION("applying doubled doubled identity function works") {
    expression<int> test = apply(apply(apply(doubler, doubler), id_f), 17);
    auto result = test.normal_form();
    REQUIRE(holds_alternative<normal_type::apply_to_primitive>(result));
    REQUIRE(get<normal_type::apply_to_primitive>(result).head == 17);
    REQUIRE(get<normal_type::apply_to_primitive>(result).args.size() == 0);
  }

}
