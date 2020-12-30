#include "../fn_once.hpp"
#include <catch.hpp>

using namespace utility;

TEST_CASE("fn_once works") {
  fn_once<int(int)> f = [](int x){ return x*x; };
  REQUIRE(std::move(f)(5) == 25);

  fn_once<int()> f2 = [x = std::make_unique<int>(17)]() { return *x; };
  REQUIRE(std::move(f2)() == 17);
}
