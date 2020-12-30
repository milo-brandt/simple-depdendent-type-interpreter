#include <catch.hpp>
#include "../expression_routine.hpp"

using namespace expressionz;
using namespace expressionz::coro;

expression_routine<int, int> sum_leafs_of(handles::expression e) {
  auto v = co_await actions::expand(e);
  if(auto* a = std::get_if<handles::apply>(&v)) {
    auto lhs = co_await sum_leafs_of(a->f);
    auto rhs = co_await sum_leafs_of(a->x);
    co_return lhs + rhs;
  } else if(auto* a = std::get_if<handles::abstract>(&v)) {
    co_return co_await sum_leafs_of(a->body);
  } else if(auto* a = std::get_if<handles::argument>(&v)) {
    co_return 0;
  } else {
    auto val = std::get<int>(v);
    co_return val;
  }
}
expression_routine<int, std::monostate> simplify_lambdas(handles::expression e) {
  auto v = co_await actions::expand(e);
  if(auto* a = std::get_if<handles::apply>(&v)) {
    co_await simplify_lambdas(a->f);
    auto lhs = co_await actions::expand(a->f);
    if(auto* l = std::get_if<handles::abstract>(&lhs)) {
      auto lambda_body = co_await actions::collapse(l->body);
      auto arg = co_await actions::collapse(a->x);
      co_await actions::replace<int>(e, substitute(arg, lambda_body));
      co_await simplify_lambdas(e);
    }
  }
  co_return std::monostate{};
}

TEST_CASE("expression routine can be manually controlled") {
  auto start = sum_leafs_of(handles::expression{0}).resume();
  REQUIRE(std::holds_alternative<external_states::expand<int, int> >(start));
  REQUIRE(std::get<external_states::expand<int, int> >(start).term.index == 0);
  auto v1 = std::move(std::get<external_states::expand<int, int> >(start)).resume(handles::apply{
    handles::expression{1},
    handles::expression{2}
  });
  REQUIRE(std::holds_alternative<external_states::expand<int, int> >(v1));
  REQUIRE(std::get<external_states::expand<int, int> >(v1).term.index == 1);
  auto v2 = std::move(std::get<external_states::expand<int, int> >(v1)).resume(17);
  REQUIRE(std::holds_alternative<external_states::expand<int, int> >(v2));
  REQUIRE(std::get<external_states::expand<int, int> >(v2).term.index == 2);
  auto v3 = std::move(std::get<external_states::expand<int, int> >(v2)).resume(34);
  REQUIRE(std::holds_alternative<int>(v3));
  REQUIRE(std::get<int>(v3) == 51);
}
TEST_CASE("run_routine can handle non-mutating routines") {
  expression<int> expr = apply(17, 34);
  auto v = run_routine(sum_leafs_of, expr);
  REQUIRE(v.second.has_value());
  REQUIRE(*v.second == 51);
  auto local_e = v.first.local_form(); //check that the expression came back out correctly
  REQUIRE(holds_alternative<local_type::apply>(local_e));
  auto local_f = get<local_type::apply>(local_e).f.local_form();
  auto local_x = get<local_type::apply>(local_e).x.local_form();
  REQUIRE(holds_alternative<local_type::value>(local_f));
  REQUIRE(get<local_type::value>(local_f) == 17);
  REQUIRE(holds_alternative<local_type::value>(local_x));
  REQUIRE(get<local_type::value>(local_x) == 34);
  SECTION("__deep_literal_compare give same result") {
    REQUIRE(__deep_literal_compare<int>(apply(17, 34), v.first));
  }
}
TEST_CASE("run_routine can handle routines using replace and collapse") {
  expression<int> expr = apply(abstract(argument(0)), 17);
  auto [e, _] = run_routine(simplify_lambdas, expr);
  auto local_e = e.local_form();
  REQUIRE(holds_alternative<local_type::value>(local_e));
  REQUIRE(get<local_type::value>(local_e) == 17);
}
