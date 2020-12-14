#ifndef COMBINATORIAL_PARSER_HPP
#define COMBINATORIAL_PARSER_HPP

#include <variant>
#include <concepts>
#include <string>
#include <tuple>
#include <vector>
#include <cctype>
#include <cassert>
#include <functional>
//#include "template_utility.hpp"

namespace parse_results {
  template<class T>
  struct success {
    T ret;
    std::string_view remaining;
  };
  struct failure{};
};
template<class T>
using parse_result = std::variant<parse_results::failure, parse_results::success<T> >;

struct recognizer {
  std::string what;
  using return_type = std::monostate;
  parse_result<std::monostate> parse(std::string_view) const;
};/*
struct alnum_t {
  using return_type = std::monostate;
  parse_result<std::monostate> parse(std::string_view) const;
};
constexpr alnum_t alnum{};
struct alpha_t {
  using return_type = std::monostate;
  parse_result<std::monostate> parse(std::string_view) const;
};
constexpr alpha_t alpha{};
*/
template<class test_t>
struct predicate {
  test_t test;
  using return_type = char;
  parse_result<char> parse(std::string_view str) const {
    if(str.empty() || !test(str[0])) return {};
    str.remove_prefix(1);
    return parse_results::success<char>{str[0], str};
  }
};
template<class test_t>
predicate(test_t) -> predicate<test_t>;
constexpr auto whitespace = predicate{static_cast<int(*)(int)>(&std::isspace)};
constexpr auto alnum = predicate{static_cast<int(*)(int)>(&std::isalnum)};
constexpr auto alpha = predicate{static_cast<int(*)(int)>(&std::isalpha)};
constexpr auto id_continuation = predicate{[](char c){ return std::isalnum(c) || c == '_'; }};
constexpr auto id_start = predicate{[](char c){ return std::isalpha(c) || c == '_'; }};

template<class T>
concept any_parser = requires(T const& parser, std::string_view str) {
  typename std::decay_t<T>::return_type;
  { parser.parse(str) } -> std::convertible_to<parse_result<typename std::decay_t<T>::return_type> >;
};
template<any_parser... Ts>
struct either : std::tuple<Ts...> {
  using std::tuple<Ts...>::tuple;
  using return_type = std::variant<typename Ts::return_type...>;
  constexpr std::tuple<Ts...> const& get_tuple() const& { return *this; }
  constexpr std::tuple<Ts...> const& get_tuple() const&& { return std::move(*this); }
  constexpr std::tuple<Ts...>&& get_tuple() & { return *this; }
  constexpr std::tuple<Ts...>&& get_tuple() && { return std::move(*this); }

  parse_result<return_type> parse(std::string_view str) const {
    return std::apply([&]<std::size_t... Is>(std::index_sequence<Is...>){ return [&](auto const&... parsers) {
      parse_result<return_type> return_slot;
      bool found_match = ([&](auto const& parser) {
        auto result = parser.parse(str);
        if(result.index() == 0) return false;
        auto& value = std::get<1>(result);
        return_slot = parse_results::success<return_type>{return_type{std::in_place_index<Is>, std::move(value.ret)}, value.remaining};
        return true;
      }(parsers) || ...);
      return return_slot;
    };}(std::index_sequence_for<Ts...>{}), get_tuple());
  }
};
template<class... Ts>
either(Ts...) -> either<Ts...>;
template<any_parser... Ts>
struct sequence : std::tuple<Ts...> {
  using std::tuple<Ts...>::tuple;
  using return_type = std::tuple<typename Ts::return_type...>;
  constexpr std::tuple<Ts...> const& get_tuple() const& { return *this; }
  constexpr std::tuple<Ts...> const& get_tuple() const&& { return std::move(*this); }
  constexpr std::tuple<Ts...>&& get_tuple() & { return *this; }
  constexpr std::tuple<Ts...>&& get_tuple() && { return std::move(*this); }
  template<std::size_t index, class... results_found_t>
  parse_result<return_type> parse_impl(std::string_view str, results_found_t&&... results_found) const {
    if constexpr(index == sizeof...(Ts)) {
      return parse_results::success<return_type>{
        std::make_tuple(std::forward<results_found_t>(results_found)...),
        str
      };
    } else {
      auto result = std::get<index>(get_tuple()).parse(str);
      if(result.index() == 0) return {};
      auto& value = std::get<1>(result);
      return parse_impl<index + 1>(value.remaining, std::forward<results_found_t>(results_found)..., std::move(value.ret));
    }
  }
  parse_result<return_type> parse(std::string_view str) const {
    return parse_impl<0>(str);
  }
};
template<class... Ts>
sequence(Ts...) -> sequence<Ts...>;

template<any_parser A, any_parser B>
constexpr auto operator+(A a, B b){ return either{std::move(a), std::move(b)}; }
template<class... Ts, any_parser B>
constexpr auto operator+(either<Ts...> a, B b) { return std::apply([](auto&&... parsers){ return either{std::move(parsers)...}; }, std::tuple_cat(std::move(a).get_tuple(),std::make_tuple(std::move(b)))); }
template<any_parser A, any_parser B>
constexpr auto operator*(A a, B b){ return sequence{std::move(a), std::move(b)}; }
template<class... Ts, any_parser B>
constexpr auto operator*(sequence<Ts...> a, B b) { return std::apply([](auto&&... parsers){ return sequence{std::move(parsers)...}; }, std::tuple_cat(std::move(a).get_tuple(),std::make_tuple(std::move(b)))); }

template<any_parser T>
struct kleene {
  T parser;
  using return_type = std::vector<typename T::return_type>;
  parse_result<return_type> parse(std::string_view str) const {
    return_type ret;
    while(true) {
      auto result = parser.parse(str);
      if(result.index() == 0) return parse_results::success<return_type>{std::move(ret), str};
      auto& value = std::get<1>(result);
      assert(str.data() != value.remaining.data()); //avoid infinite loops
      str = value.remaining;
      ret.push_back(std::move(value.ret));
    }
  }
};
template<class T>
kleene(T) -> kleene<T>;
template<any_parser T>
constexpr auto operator*(T parser){ return kleene{std::move(parser)}; }
template<any_parser T, std::regular_invocable<typename T::return_type> F>
struct fmap {
  T parser;
  F map;
  using return_type = std::invoke_result_t<F, typename T::return_type>;
  parse_result<return_type> parse(std::string_view str) const {
    auto result = parser.parse(str);
    if(result.index() == 0) return {};
    auto& value = std::get<1>(result);
    return parse_results::success<return_type>{map(std::move(value.ret)), value.remaining};
  }
};
template<class T, class F>
fmap(T, F) -> fmap<T, F>;
template<any_parser T, std::regular_invocable<typename T::return_type> F>
constexpr auto operator>(T parser, F map){ return fmap{std::move(parser), std::move(map)}; }

template<class T>
struct captured_value {
  T value;
  std::string_view range;
};
template<any_parser T>
struct capture {
  T parser;
  using return_type = captured_value<typename T::return_type>;
  parse_result<return_type> parse(std::string_view str) const {
    auto result = parser.parse(str);
    if(result.index() == 0) return {};
    auto& value = std::get<1>(result);
    return parse_results::success<return_type>{{std::move(value.ret), std::string_view{str.data(), (std::size_t)(value.remaining.data() - str.data())}}, value.remaining};
  }
};
template<class T>
capture(T) -> capture<T>;

constexpr auto get_second_value_of_tuple = []<class A, class B>(std::tuple<A, B> tup) {
  return std::move(std::get<1>(tup));
};
template<any_parser A, any_parser B>
struct preceded : fmap<sequence<A, B>, decltype(get_second_value_of_tuple)> {
  constexpr preceded(A a, B b):fmap<sequence<A, B>, decltype(get_second_value_of_tuple)>{{std::move(a), std::move(b)}, {}}{}
};
template<class A, class B>
preceded(A, B) -> preceded<A, B>;
constexpr auto get_first_value_of_tuple = []<class A, class B>(std::tuple<A, B> tup) {
  return std::move(std::get<0>(tup));
};
template<any_parser A, any_parser B>
struct suffixed : fmap<sequence<A, B>, decltype(get_first_value_of_tuple)> {
  constexpr suffixed(A a, B b):fmap<sequence<A, B>, decltype(get_first_value_of_tuple)>{{std::move(a), std::move(b)}, {}}{}
};
template<class A, class B>
suffixed(A, B) -> suffixed<A, B>;
constexpr auto get_middle_value_of_tuple = []<class A, class B, class C>(std::tuple<A, B, C> tup) {
  return std::move(std::get<1>(tup));
};
template<any_parser A, any_parser B, any_parser C>
struct surrounded : fmap<sequence<A, B, C>, decltype(get_middle_value_of_tuple)> {
  constexpr surrounded(A a, B b, C c):fmap<sequence<A, B, C>, decltype(get_middle_value_of_tuple)>{{std::move(a), std::move(b), std::move(c)}, {}}{}
};
template<class A, class B, class C>
surrounded(A,B,C) -> surrounded<A, B, C>;
template<any_parser T>
struct ref {
  T const* r;
  using return_type = typename T::return_type;
  constexpr ref(T const& r):r(&r){}
  ref(T&&) = delete;
  ref(T const&&) = delete;
  parse_result<return_type> parse(std::string_view str) const {
    return r->parse(str);
  }
};
template<any_parser T, class F> requires requires(F f, typename T::return_type r){ {f(r)} -> any_parser; }
struct bind {
  T parser;
  F func;
  using return_type = typename decltype(std::declval<F>()(std::declval<typename T::return_type>()))::return_type;
  parse_result<return_type> parse(std::string_view str) const {
    auto result = parser.parse(str);
    if(result.index() == 0) return {};
    auto& value = std::get<1>(result);
    return func(value.ret).parse(value.remaining);
  }
};
template<any_parser T, class F> requires requires(F f, typename T::return_type r){ {f(r)} -> any_parser; }
constexpr auto operator>>(T parser, F func) { return bind{std::move(parser), std::move(func)}; }
template<any_parser T>
struct optional {
  T parser;
  using return_type = std::optional<typename T::return_type>;
  parse_result<return_type> parse(std::string_view str) const {
    auto result = parser.parse(str);
    if(result.index() == 0) return parse_results::success<return_type>{std::nullopt, str};
    auto& value = std::get<1>(result);
    return parse_results::success<return_type>{value.ret, value.remaining};
  }
};
template<class T>
optional(T) -> optional<T>;
template<any_parser T>
constexpr auto operator-(T parser){ return optional{std::move(parser)}; }

template<class T>
ref(T const&) -> ref<T>;
template<class term_t, class separator_t>
struct separated_sequence {
  term_t term;
  separator_t sep;
  using return_type = std::vector<typename term_t::return_type>;
  parse_result<return_type> parse(std::string_view str) const {
    return_type ret;
    while(true) {
      auto result = term.parse(str);
      if(result.index() == 0) return parse_results::success<return_type>{std::move(ret), str}; //TODO: It's not clear this is proper behavior
      auto& value = std::get<1>(result);
      assert(str.data() != value.remaining.data()); //avoid infinite loops
      str = value.remaining;
      ret.push_back(std::move(value.ret));
      auto sep_result = sep.parse(str);
      if(sep_result.index() == 0) return parse_results::success<return_type>{std::move(ret), str};
      str = std::get<1>(sep_result).remaining;
    }
  }
};
template<class T>
struct pure {
  T ret;
  using return_type = T;
  parse_result<return_type> parse(std::string_view str) const { return parse_results::success<return_type>{ret, str}; }
};
template<class T>
pure(T) -> pure<T>;
template<class T>
struct maybe_pure {
  std::optional<T> ret;
  maybe_pure(std::optional<T> ret):ret(std::move(ret)){}
  maybe_pure(T ret):ret(std::move(ret)){}
  maybe_pure() = default;
  using return_type = T;
  parse_result<return_type> parse(std::string_view str) const {
    if(ret) return parse_results::success<return_type>{*ret, str};
    else return {};
  }
};
template<class T>
maybe_pure(std::optional<T>) -> maybe_pure<T>;
template<class T>
maybe_pure(T) -> maybe_pure<T>;
template<class F> requires requires(F const& f){ {f()} -> any_parser; }
struct lazy {
  F f;
  using return_type = typename decltype(std::declval<F const&>()())::return_type;
  parse_result<return_type> parse(std::string_view str) const {
    return f().parse(str);
  }
};
template<class F>
lazy(F) -> lazy<F>;
template<class term_t, class separator_t>
separated_sequence(term_t, separator_t) -> separated_sequence<term_t, separator_t>;
template<class T>
struct fail_t {
  using return_type = T;
  parse_result<T> parse(std::string_view str) const {
    return {};
  }
};
template<class T>
constexpr fail_t<T> fail{};
template<class R>
struct dynamic_parser {
  using return_type = R;
  std::function<parse_result<R>(std::string_view)> parse;
  template<any_parser T> requires std::is_same_v<typename T::return_type, R>
  dynamic_parser(T parser):parse([parser = std::move(parser)](std::string_view str){ return parser.parse(str); }){}
};

static constexpr auto identifier = capture{id_start * *id_continuation} > [](auto ret){ return ret.range; };

#endif
