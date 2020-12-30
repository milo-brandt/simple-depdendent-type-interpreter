#include "../lifted_state_machine.hpp"
#include <catch.hpp>

struct take_int_t{};
constexpr take_int_t take_int{};
struct yield_int{
  int x;
};
struct simple_definition : lifted_state_machine::coro_base<simple_definition> {
  using starter_base = resume_handle<>;
  struct state{};
  using paused_type = std::variant<std::pair<int, resume_handle<> >, resume_handle<int>, int>;
  static starter_base create_object(resume_handle<> handle) {
    return handle;
  }
  static auto on_await(take_int_t, state&, waiting_handle&& handle) {
    return std::move(handle).suspending_result<int>([](auto resumer){ return resumer; });
  }
  static auto on_yield(int x, state&, waiting_handle&& handle) {
    return std::move(handle).suspending_result([&](auto resumer){ return std::make_pair(x, std::move(resumer)); });
  }
  static void on_return_void(state&, returning_handle&& handle) {
    return std::move(handle).result(17);
  }
};
lifted_state_machine::coro_type<simple_definition> simple_routine() {
  int acc = 0;
  while(true) {
    acc += co_await take_int;
    if(acc == 289) throw int(5);
    if(acc > 500) acc = 1;
    co_yield acc;
  }
}
TEST_CASE("Machine correctly simulates process pushing and pulling integers") {
  auto r = simple_routine();
  auto x0 = std::move(r).resume();
  REQUIRE(x0.index() == 1);
  auto x1 = std::move(std::get<1>(x0)).resume(17);
  REQUIRE(x1.index() == 0);
  REQUIRE(std::get<0>(x1).first == 17);
  auto x2 = std::move(std::get<0>(x1)).second.resume();
  REQUIRE(x2.index() == 1);
  auto x3 = std::move(std::get<1>(x2)).resume(34);
  REQUIRE(x3.index() == 0);
  REQUIRE(std::get<0>(x3).first == 51);
  auto x4 = std::move(std::get<0>(x3)).second.resume();
  REQUIRE(x4.index() == 1);
  REQUIRE_THROWS(std::move(std::get<1>(x4)).resume(289 - 51));
}
