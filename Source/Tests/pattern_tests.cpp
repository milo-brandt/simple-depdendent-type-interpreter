#include <catch.hpp>
#include "../pattern_matcher.hpp"

using namespace patterns;

TEST_CASE("match with arg and discard work") {
  int x = 0;
  SECTION("arg is captured") {
    match(5, pattern(arg) >> [&](int y){ x = y; });
    REQUIRE(x == 5);
  }
  SECTION("discard is called") {
    match(5, pattern(discard) >> [&](){ x = 17; });
    REQUIRE(x == 17);
  }
  SECTION("first matching argument is called") {
    match(5,
      pattern(discard) >> [&](){ x = 17; },
      pattern(arg) >> [&](int y){ x = y; }
    );
    REQUIRE(x == 17);
  }
}
TEST_CASE("match_result with arg and discard work") {
  SECTION("arg is captured") {
    REQUIRE(match_result(5, pattern(arg) >> [&](int y){ return y; }) == 5);
  }
  SECTION("arg is captured with explicit type given") {
    REQUIRE(match_result<int>(5, pattern(arg) >> [&](int y){ return y; }) == 5);
  }
  SECTION("discard is called") {
    REQUIRE(match_result(5, pattern(discard) >> [&](){ return 17; }) == 17);
  }
  SECTION("first matching argument is called") {
    REQUIRE(match_result(5,
      pattern(discard) >> [&](){ return 17; },
      pattern(arg) >> [&](int y){ return y; }
    ) == 17);
  }
}
