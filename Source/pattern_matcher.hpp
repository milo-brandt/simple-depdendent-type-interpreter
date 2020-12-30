#ifndef PATTERN_MATCHER_HPP
#define PATTERN_MATCHER_HPP

#include <optional>
#include <tuple>

namespace patterns {
  template<class T>
  struct pattern {
    T data;
    pattern(T data):data(std::move(data)){}
  };
  template<class T>
  pattern(T) -> pattern<T>;
  template<class T, class F>
  struct match_arm {
    pattern<T> pat;
    F callback;
    template<class A>
    bool matches(A&& a) const{ return pat.data.matches(std::forward<A>(a)); }
    template<class A>
    decltype(auto) act(A&& a) { return std::apply(std::move(callback), pat.data.get_tuple(std::forward<A>(a))); }
  };
  template<class T, class F>
  match_arm<T, F> operator>>(pattern<T> pat, F call) {
    return {std::move(pat), std::move(call)};
  }
  struct arg_t{
    template<class A>
    bool matches(A&&) const{ return true; }
    template<class A>
    auto get_tuple(A&& arg) const{ return std::make_tuple(std::forward<A>(arg)); }
  };
  constexpr arg_t arg{};
  struct discard_t{
    template<class A>
    bool matches(A&&) const{ return true; }
    template<class A>
    auto get_tuple(A&&){ return std::make_tuple(); }
  };
  constexpr discard_t discard;
  template<class... Ts>
  struct tuple{
    std::tuple<Ts...> subpatterns;
    constexpr tuple(Ts... ts):subpatterns(ts...){}
    template<class... As> requires (sizeof...(As) == sizeof...(Ts))
    bool matches(std::tuple<As...> const& a) const {
      return std::apply([&](auto const&... pats) {
        return std::apply([&](auto const&... as) {
          return (pats.matches(as) && ...);
        }, a);
      }, subpatterns);
    }
    template<class... As> requires (sizeof...(As) == sizeof...(Ts))
    bool get_tuple(std::tuple<As...> const& a) const {
      return std::apply([&](auto const&... pats) {
        return std::apply([&](auto const&... as) {
          return std::tuple_cat(pats.get_tuple(as)...);
        }, a);
      }, subpatterns);
    }
  };

  template<class A, class... Ts>
  auto match(A&& value, Ts&&... ts) {
    (void)((ts.matches(value) ? (void)ts.act(std::forward<A>(value)) , true : false) || ...);
  }
  template<class R, class A, class T, class... Ts>
  R match_result(A&& value, T&& t, Ts&&... ts) {
    if(t.matches(value)) {
      return t.act(std::forward<A>(value));
    }
    if constexpr(sizeof...(Ts) == 0) {
      std::terminate(); //nothing found!
    } else {
      return match_result(std::forward<A>(value), std::forward<Ts>(ts)...);
    }
  }
  namespace detail {
    template<class M, class A>
    using match_result_type = decltype(std::declval<M>().act(std::declval<A>()));
  }
  template<class A, class... Ts, class R = std::common_type_t<detail::match_result_type<Ts, A&&>...> > requires (sizeof...(Ts) > 0)
  R match_result(A&& value, Ts&&... ts) {
    return match_result<R>(std::forward<A>(value), std::forward<Ts>(ts)...);
  }
}
/*
match(x,
  pattern{arg} >> [&](auto x) { std::cout << ... << "\n" },

)
*/

#endif
