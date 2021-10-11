#include "../Utility/parts.hpp"
#include <catch.hpp>
#include <tuple>
#include <array>
#include <vector>

namespace test_case {
  struct A {
    int x;
    int y;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct B {
    int z;
    A value_1;
    A value_2;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct C {
    static constexpr auto part_info = mdb::parts::simple<0>;
  };
  struct D {
    A v1;
    B v2;
    C v3;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
};

TEST_CASE("Parts are recognized for a small struct.") {
  test_case::A value{
    .x = 5,
    .y = 8
  };
  int sum = 0;
  mdb::parts::visit_children(value, [&sum](int child) {
    sum += child;
  });
  REQUIRE(sum == 13);
}
template<class T>
int sum_all_children_of(T& value) {
  if constexpr(std::is_same_v<std::decay_t<T>, int>) {
    return value;
  } else {
    int sum = 0;
    mdb::parts::visit_children(value, [&sum](auto& child) { sum += sum_all_children_of(child); });
    return sum;
  }
}
TEST_CASE("Parts are recognized in a recursive definition.") {
  test_case::D value{
    .v1 = {
      .x = 1,
      .y = 2
    },
    .v2 = {
      .z = 3,
      .value_1 = {
        .x = 4,
        .y = 5
      },
      .value_2 = {
        .x = 6,
        .y = 7
      }
    },
    .v3{}
  };
  REQUIRE(sum_all_children_of(value) == 28);
}
TEST_CASE("Parts are recognized for tuple-like types in standard library.") {
  int sum = 0;
  auto tuple = std::make_tuple(1, 5, 8);
  mdb::parts::visit_children(tuple, [&sum](int child) {
    sum += child;
  });
  REQUIRE(sum == 14);
  sum = 0;
  auto pair = std::make_pair(5, 4);
  mdb::parts::visit_children(pair, [&sum](int child) {
    sum += child;
  });
  REQUIRE(sum == 9);
  sum = 0;
  std::array<int, 4> array{1, 2, 3, 4};
  mdb::parts::visit_children(array, [&sum](int child) {
    sum += child;
  });
  REQUIRE(sum == 10);
}
TEST_CASE("Parts are recognized for ranges in standard library.") {
  int sum = 0;
  std::vector<int> vec{1,2,3,4,5};
  mdb::parts::visit_children(vec, [&sum](int child) {
    sum += child;
  });
  REQUIRE(sum == 15);
}
class E {
  int x;
  int y;
public:
  E(int x, int y):x(x),y(y) {}
  static constexpr auto part_info = mdb::parts::apply_call([](auto& v, auto&& callback) { callback(v.x, v.y); });
};
TEST_CASE("The part_info member can be specified as apply_call") {
  int sum = 0;
  E val{8, 9};
  mdb::parts::visit_children(val, [&sum](int child) {
    sum += child;
  });
  REQUIRE(sum == 17);
}
TEST_CASE("Variants are destructured to the active element") {
  int sum = 0;
  std::variant<int, bool> value{true};
  mdb::parts::visit_children(value, [&sum]<class T>(T& v) {
    if constexpr(std::is_same_v<T, int>) {
      sum = v;
    } else {
      sum = (v ? 17 : 34);
    }
  });
  REQUIRE(sum == 17);
}
