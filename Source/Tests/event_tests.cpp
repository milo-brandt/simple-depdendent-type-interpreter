#include "../Utility/event.hpp"
#include <catch.hpp>

TEST_CASE("Events work at a basic level.") {
  mdb::Event<int> new_value;
  int x = 0;
  new_value.listen([&x](int v) {
    x = v;
  });
  for(int i = 1; i < 100; ++i) {
    new_value(i);
    REQUIRE(x == i);
  }
}
TEST_CASE("Listeners can be disconnected from events.") {
  mdb::Event<int> new_value;
  int x = 5;
  new_value(78);
  auto hook = new_value.listen([&x](int y) { x = y; });
  REQUIRE(x == 5);
  new_value(8);
  REQUIRE(x == 8);
  std::move(hook).disconnect();
  new_value(9);
  REQUIRE(x == 8);
}
TEST_CASE("The event class destroys disconnected listeners.") {
  struct SetOnDestroy {
    int& target;
    void operator()() {}
    ~SetOnDestroy() { target = 1; }
  };
  int x = 0;
  {
    mdb::Event<> dummy;
    dummy.listen(SetOnDestroy{x});
    x = 0;
  }
  REQUIRE(x == 1);
  {
    mdb::Event<> dummy;
    auto hook = dummy.listen(SetOnDestroy{x});
    x = 0;
    std::move(hook).disconnect();
    REQUIRE(x == 1);
    x = 0;
  }
  REQUIRE(x == 0); //ensure no double destruction
}
TEST_CASE("Listeners can disconnect themselves from events while executing.") {
  mdb::Event<int> new_value;
  int x = 0;
  mdb::Event<int>::Hook hook = new_value.listen([&hook, &x](int v) {
    x = v;
    if(v == 2) {
      std::move(hook).disconnect();
    }
  });
  new_value(1);
  REQUIRE(x == 1);
  new_value(2);
  REQUIRE(x == 2);
  new_value(3);
  REQUIRE(x == 2);
  new_value(4);
  REQUIRE(x == 2);
}
TEST_CASE("Listeners are called in the order of connection.") {
  mdb::Event<int> new_value;
  int x = 0;
  new_value.listen([&x](int y) { x = y; });
  new_value.listen([&x](int y) { x = 2*y; });
  for(int i = 1; i < 10; ++i) {
    new_value(i);
    REQUIRE(x == 2 * i);
  }
}
