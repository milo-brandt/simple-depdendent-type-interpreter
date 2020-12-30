#include <catch.hpp>
#include "../result.hpp"

result_routine<int, int> process_integer(int x, int y, int& ct) {
  ++ct;
  if(x == 17) co_yield y;
  co_return x;
}
result_routine<int, int> process_sum(int x, int y, int& ct) {
  int first = co_await process_integer(x, 1, ct);
  int second = co_await process_integer(y, 2, ct);
  co_return first + second;
}

TEST_CASE("Result routines work") {
  int ct = 0;
  SECTION("Gives result if no errors are reported") {
    result<int, int> r = process_sum(1, 2, ct);
    REQUIRE(r.as_variant.index() == 0);
    REQUIRE(std::get<0>(r.as_variant) == 3);
    REQUIRE(ct == 2);
  }
  SECTION("Fails if an errors is reported on first argument") {
    result<int, int> r = process_sum(17, 2, ct);
    REQUIRE(r.as_variant.index() == 1);
    REQUIRE(std::get<1>(r.as_variant) == 1);
    REQUIRE(ct == 1);
  }
  SECTION("Fails if an errors is reported on second argument") {
    result<int, int> r = process_sum(2, 17, ct);
    REQUIRE(r.as_variant.index() == 1);
    REQUIRE(std::get<1>(r.as_variant) == 2);
    REQUIRE(ct == 2);
  }
  SECTION("Fails on first argument if an errors is reported on both arguments") {
    result<int, int> r = process_sum(17, 17, ct);
    REQUIRE(r.as_variant.index() == 1);
    REQUIRE(std::get<1>(r.as_variant) == 1);
    REQUIRE(ct == 1);
  }
}
