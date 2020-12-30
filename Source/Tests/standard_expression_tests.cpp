#include <catch.hpp>
#include "../standard_expression.hpp"
#include <sstream>

using namespace expressionz::standard;
using namespace expressionz::coro;
using namespace expressionz;

routine<std::optional<standard_expression> > inductor_rule(simplify_routine_t const& s, std::vector<handles::expression> args) {
  assert(args.size() == 2);
  auto second_arg = co_await s(args[1]);
  if(!second_arg || second_arg->args.size() != 2) co_return std::nullopt;
  co_return apply(apply(
    co_await actions::collapse(args[0]),
    co_await actions::collapse(second_arg->args[0])),
    co_await actions::collapse(second_arg->args[1])
  );
}

TEST_CASE("pattern matching on pair works") {
  context ctx;
  auto pair = ctx.register_axiom({"pair"});
  auto inductor = ctx.register_declaration({"induct"});
  //induct f (pair x y) := f x y
  ctx.add_rule(inductor, {2, inductor_rule});

  standard_expression first_arg = abstract(standard_expression(abstract(argument(1))));
  standard_expression second_arg = abstract(standard_expression(abstract(argument(0))));
  standard_expression pair_instance = apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34);


  SECTION("can extract first argument") {
    standard_expression get_first = apply(apply(inductor, first_arg), pair_instance);
    std::stringstream str;
    str << "Reduction sequence:\n\t";
    run_routine(simple_output, get_first, ctx, str); str << "\n";
    auto [e, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
      str << "\t";
      exec(simple_output(outer, ctx, str));
      str << "\n";
    }, simplify_outer, get_first, ctx);
    INFO(str.str());
    auto e_l = e.local_form();
    REQUIRE(holds_alternative<local_type::value>(e_l));
    REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
    REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 17);
  }
  SECTION("can extract second argument") {
    standard_expression get_second = apply(apply((primitives::any)inductor, second_arg), pair_instance);
    auto e_l = run_routine(simplify_outer, get_second, ctx).first.local_form();
    REQUIRE(holds_alternative<local_type::value>(e_l));
    REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
    REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 34);
  }
}

routine<std::optional<standard_expression> > nat_inductor_rule(primitives::declaration inductor, simplify_routine_t const& s, std::vector<handles::expression> args) {
  assert(args.size() == 3);
  auto n_arg = co_await s(args[2]);
  if(!n_arg) co_return std::nullopt;
  if(n_arg->args.size() == 0) {
    co_return co_await actions::collapse(args[1]);
  } else {
    auto f = co_await actions::collapse(args[0]);
    auto p = co_await actions::collapse(args[1]);
    auto n = co_await actions::collapse(n_arg->args[0]);
    co_return apply(apply(f, n), apply(apply(apply(inductor, f), p), n));
  }
}
TEST_CASE("pattern matching on nat works") {
  context ctx;
  auto zero = ctx.register_axiom({"zero"});
  auto succ = ctx.register_axiom({"succ"});
  auto inductor = ctx.register_declaration({"induct"});
  ctx.add_rule(inductor, {3, [&](auto const& s, auto&& args){ return nat_inductor_rule(inductor, s, std::move(args)); }});
  standard_expression sum = apply(inductor, abstract(succ));
  SECTION("can sum") {
    standard_expression fiver = apply(apply(sum, apply(succ, apply(succ, apply(succ, zero)))), apply(succ, apply(succ, zero)));
    std::stringstream str;
    str << "Reduction sequence:\n\t";
    run_routine(simple_output, fiver, ctx, str); str << "\n";
    auto [e, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
      str << "\t";
      exec(simple_output(outer, ctx, str));
      str << "\n";
    }, simplify_total, fiver, ctx);
    INFO(str.str());
    REQUIRE(__deep_literal_compare<primitives::any>(e, apply(succ, apply(succ, apply(succ, apply(succ, apply(succ, zero)))))));
  }
  SECTION("can multiply") {
    standard_expression times = abstract(apply(apply(inductor, abstract(apply(sum, argument(1)))), zero));
    standard_expression sixer = apply(apply(times, apply(succ, apply(succ, apply(succ, zero)))), apply(succ, apply(succ, zero)));
    std::stringstream str;
    str << "Reduction sequence:\n\t";
    run_routine(simple_output, sixer, ctx, str); str << "\n";
    auto [e, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
      str << "\t";
      exec(simple_output(outer, ctx, str));
      str << "\n";
    }, simplify_total, sixer, ctx);
    INFO(str.str());
    REQUIRE(__deep_literal_compare<primitives::any>(e, apply(succ, apply(succ, apply(succ, apply(succ, apply(succ, apply(succ, zero))))))));
  }
  SECTION("can accumulate") {
    standard_expression acc = apply(apply(inductor, abstract(apply(sum, argument(0)))), zero);
    standard_expression sixer = apply(acc,apply(succ, apply(succ, apply(succ, apply(succ, zero)))));
    std::stringstream str;
    str << "Reduction sequence:\n\t";
    run_routine(simple_output, sixer, ctx, str); str << "\n";
    auto [e, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
      str << "\t";
      exec(simple_output(outer, ctx, str));
      str << "\n";
    }, simplify_total, sixer, ctx);
    INFO(str.str());
    REQUIRE(__deep_literal_compare<primitives::any>(e, apply(succ, apply(succ, apply(succ, apply(succ, apply(succ, apply(succ, zero))))))));
  }
}

template<std::size_t index>
routine<std::optional<standard_expression> > take_arg_rule(simplify_routine_t const& s, std::vector<handles::expression> args) {
  assert(args.size() > index);
  co_return co_await actions::collapse(args[index]);
}
TEST_CASE("rules defined by pattern matching work") {
  context ctx;
  auto pair = ctx.register_axiom({"pair"});
  auto inductor = ctx.register_declaration({"induct"});
  SECTION("no-op pattern works") {
    auto [decl, rule] = create_pattern_rule(apply(inductor, argument(0)), take_arg_rule<0>, ctx);
    ctx.add_rule(decl, std::move(rule));
    standard_expression expr = apply(inductor, (std::uint64_t)17);
    auto e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
    REQUIRE(holds_alternative<local_type::value>(e_l));
    REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
    REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 17);
    expr = apply(inductor, expr);
    e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
    REQUIRE(holds_alternative<local_type::value>(e_l));
    REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
    REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 17);
  }
  SECTION("selector pattern works") {
    auto [decl, rule] = create_pattern_rule(apply(inductor, apply(apply(pair, argument(0)), argument(1))), take_arg_rule<0>, ctx);
    ctx.add_rule(decl, std::move(rule));
    standard_expression pair_instance = apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34);
    standard_expression expr = apply(inductor, pair_instance);
    auto e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
    REQUIRE(holds_alternative<local_type::value>(e_l));
    REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
    REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 17);
    expr = apply(inductor, (std::uint64_t)17);
    auto e = run_routine(simplify_outer, expr, ctx).first;
    REQUIRE(__deep_literal_compare(e, expr));
  }
  SECTION("selector pattern works on second arg") {
    auto [decl, rule] = create_pattern_rule(apply(inductor, apply(apply(pair, argument(0)), argument(1))), take_arg_rule<1>, ctx);
    ctx.add_rule(decl, std::move(rule));
    standard_expression pair_instance = apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34);
    standard_expression expr = apply(inductor, pair_instance);
    auto e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
    REQUIRE(holds_alternative<local_type::value>(e_l));
    REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
    REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 34);
  }
  SECTION("deep selector pattern works") {
    SECTION("on first argument") {
      auto [decl, rule] = create_pattern_rule(apply(inductor, apply(apply(pair, apply(apply(pair, argument(0)), argument(1))), argument(2))), take_arg_rule<0>, ctx);
      ctx.add_rule(decl, std::move(rule));
      standard_expression expr = apply(inductor, apply(apply(pair, apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34)), (std::uint64_t)51));
      auto e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
      REQUIRE(holds_alternative<local_type::value>(e_l));
      REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
      REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 17);
    }
    SECTION("on second argument") {
      auto [decl, rule] = create_pattern_rule(apply(inductor, apply(apply(pair, apply(apply(pair, argument(0)), argument(1))), argument(2))), take_arg_rule<1>, ctx);
      ctx.add_rule(decl, std::move(rule));
      standard_expression expr = apply(inductor, apply(apply(pair, apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34)), (std::uint64_t)51));
      auto e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
      REQUIRE(holds_alternative<local_type::value>(e_l));
      REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
      REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 34);
    }
    SECTION("on third argument") {
      auto [decl, rule] = create_pattern_rule(apply(inductor, apply(apply(pair, apply(apply(pair, argument(0)), argument(1))), argument(2))), take_arg_rule<2>, ctx);
      ctx.add_rule(decl, std::move(rule));
      standard_expression expr = apply(inductor, apply(apply(pair, apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34)), (std::uint64_t)51));
      auto e_l = run_routine(simplify_outer, expr, ctx).first.local_form();
      REQUIRE(holds_alternative<local_type::value>(e_l));
      REQUIRE(std::holds_alternative<std::uint64_t>(get<local_type::value>(e_l)));
      REQUIRE(std::get<std::uint64_t>(get<local_type::value>(e_l)) == 51);
    }
  }
}
TEST_CASE("rules defined by pattern replacement work") {
  context ctx;
  auto pair = ctx.register_axiom({"pair"});
  auto inductor = ctx.register_declaration({"induct"});
  auto [decl, rule] = create_pattern_replacement_rule(
    apply(inductor, apply(apply(pair, argument(0)), argument(1))),
    apply(argument(0), argument(1)),
    ctx
  );
  ctx.add_rule(decl, std::move(rule));
  standard_expression expr = apply(inductor, apply(apply(pair, (std::uint64_t)17), (std::uint64_t)34));
  auto e = run_routine(simplify_outer, expr, ctx).first;
  REQUIRE(__deep_literal_compare(e, (standard_expression)apply((std::uint64_t)17, (std::uint64_t)34)));
}
