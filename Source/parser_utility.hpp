#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <unordered_set>
#include "lifted_state_machine.hpp"

namespace parse {
  struct parser_traceback {
    std::string_view position;
    std::string reason;
    std::vector<std::pair<std::string, std::string_view> > stack;
    parser_traceback(std::string_view position, std::string reason):position(position),reason(std::move(reason)){}
  };
  namespace parse_result {
    struct not_matched : parser_traceback {
      using parser_traceback::parser_traceback;
      not_matched(parser_traceback t):parser_traceback(std::move(t)){}
    };
    struct failed : parser_traceback {
      using parser_traceback::parser_traceback;
      failed(parser_traceback t):parser_traceback(std::move(t)){}
    };
    template<class T>
    struct success {
      std::string_view position;
      T value;
    };
    template<class T>
    success(std::string_view, T) -> success<T>;
    template<class T>
    using any = std::variant<not_matched, failed, success<T> >;
  }

  template<class T>
  concept parser = requires(T v, std::string_view str) {
    std::move(v)(str);
  };
  template<class T>
  using parser_return_type = decltype(std::get<2>(std::declval<T>()(std::string_view{})).value);
  struct cut_t{};
  constexpr cut_t cut{};
  struct error_override {
    std::string name;
  };
  struct with_traceback {
    std::string name;
  };
  struct get_position_t {};
  constexpr get_position_t get_position;
  struct set_position {
    std::string_view position;
  };
  struct fail {
    std::string reason;
  };
  template<class T>
  struct parsing_function;
  template<class T>
  using parsing_routine = lifted_state_machine::coro_type<parsing_function<T> >;
  template<class T>
  struct parsing_function : lifted_state_machine::coro_base<parsing_function<T> > {
    using coro_base =  lifted_state_machine::coro_base<parsing_function<T> >;
    template<class V = void>
    using resume_handle = typename coro_base::template resume_handle<V>;
    struct state {
      std::string_view position;
      bool cut = false;
      std::optional<std::pair<std::string, std::string_view> > error_override;
      std::vector<std::pair<std::string, std::string_view> > stack;
    };
    using paused_type = parse_result::any<T>;
    class starter_base {
      resume_handle<> handle;
      friend parsing_function;
      starter_base(resume_handle<> handle):handle(std::move(handle)){}
    public:
      starter_base() = default;
      parse_result::any<T> operator()(std::string_view str) && {
        handle.state().position = str;
        return std::move(handle).resume();
      }
    };
    static starter_base create_object(resume_handle<> handle) {
      return starter_base(std::move(handle));
    }
    static auto on_await(cut_t, state& state, typename coro_base::waiting_handle&& handle) {
      state.cut = true;
      return std::move(handle).immediate_result();
    }
    static auto on_await(error_override error, state& state, typename coro_base::waiting_handle&& handle) {
      state.error_override = std::make_pair(std::move(error.name), state.position);
      state.stack.clear();
      return std::move(handle).immediate_result();
    }
    static auto on_await(with_traceback error, state& state, typename coro_base::waiting_handle&& handle) {
      state.stack.push_back(std::make_pair(std::move(error.name), state.position));
      return std::move(handle).immediate_result();
    }
    static auto on_await(get_position_t, state& state, typename coro_base::waiting_handle&& handle) {
      return std::move(handle).immediate_result(state.position);
    }
    static auto on_await(set_position set, state& state, typename coro_base::waiting_handle&& handle) {
      state.position = set.position;
      return std::move(handle).immediate_result();
    }
    static auto on_await(fail f, state& state, typename coro_base::waiting_handle&& handle) {
      return std::move(handle).template destroying_result(parse_result::failed{state.position, std::move(f.reason)});
    }

    template<parser P, class V = parser_return_type<P> >
    static coro::variant_awaiter<V, lifted_state_machine::immediate_awaiter<V>, lifted_state_machine::await_with_no_resume<V> >
      on_await(P&& routine, state& state, typename coro_base::waiting_handle&& handle) {
      auto ret = std::forward<P>(routine)(state.position);
      if(ret.index() < 2) {
        parser_traceback trace = ret.index() == 0 ? (parser_traceback&&)std::get<0>(std::move(ret)) : (parser_traceback&&)std::get<1>(std::move(ret));
        if(state.error_override) {
          trace.stack.clear();
          trace.position = state.error_override->second;
          trace.reason = state.error_override->first;
        }
        trace.stack.insert(trace.stack.end(), state.stack.rbegin(), state.stack.rend());
        if(ret.index() == 1 || state.cut) {
          return std::move(handle).template destroying_result<V>(parse_result::failed{std::move(trace)});
        } else {
          return std::move(handle).template destroying_result<V>(parse_result::not_matched{std::move(trace)});
        }
      } else {
        auto& [position, value] = std::get<2>(ret);
        state.position = position;
        return std::move(handle).immediate_result(std::move(value));
      }
    }
    static void on_return_value(T value, state& state, typename coro_base::returning_handle&& handle) {
      return std::move(handle).result(parse_result::success<T>{state.position, std::move(value)});
    }
  };
  struct tag {
    std::string what;
    inline parse_result::any<std::monostate> operator()(std::string_view str) const& {
      if(str.starts_with(what)) {
        str.remove_prefix(what.size());
        return parse_result::success<std::monostate>{str, {}};
      } else {
        return parse_result::not_matched{str, "expected \"" + what + "\""};
      }
    }
  };
  template<parser inner_t>
  struct optional {
    inner_t inner;
    using inner_ret_type = parser_return_type<inner_t>;
    using ret_type = std::optional<inner_ret_type>;
    parse_result::any<ret_type> operator()(std::string_view str) && {
      auto result = std::move(inner)(str);
      if(result.index() == 0) return parse_result::success<ret_type>{str, std::nullopt};
      if(result.index() == 1) return std::get<1>(std::move(result));
      else {
        auto& [position, value] = std::get<2>(result);
        return parse_result::success<ret_type>{position, std::move(value)};
      }
    }
  };
  template<class T>
  optional(T) -> optional<T>;
  template<parser inner_t>
  struct capture {
    inner_t inner;
    using inner_ret_type = parser_return_type<inner_t>;
    using ret_type = std::pair<inner_ret_type, std::string_view>;
    parse_result::any<ret_type> operator()(std::string_view str) && {
      auto result = std::move(inner)(str);
      if(result.index() == 0) return std::get<0>(std::move(result));
      if(result.index() == 1) return std::get<1>(std::move(result));
      else {
        auto& [position, value] = std::get<2>(result);
        return parse_result::success<ret_type>{position, std::make_pair(std::move(value), str.substr(0, position.data() - str.data()))};
      }
    }
  };
  template<class T>
  capture(T) -> capture<T>;
  template<parser inner_t, class F>
  struct fmap {
    inner_t inner;
    F map;
    using inner_ret_type = parser_return_type<inner_t>;
    using ret_type = decltype(map(std::declval<inner_ret_type>()));
    parse_result::any<ret_type> operator()(std::string_view str) && {
      auto result = std::move(inner)(str);
      if(result.index() == 0) return std::get<0>(std::move(result));
      if(result.index() == 1) return std::get<1>(std::move(result));
      else {
        auto& [position, value] = std::get<2>(result);
        return parse_result::success<ret_type>{position, map(std::move(value))};
      }
    }
  };
  template<class T, class F>
  fmap(T, F) -> fmap<T, F>;
  template<class F>
  struct char_predicate {
    F predicate;
    inline parse_result::any<char> operator()(std::string_view str) const {
      if(!str.empty() && predicate(str[0])) {
        auto c = str[0];
        str.remove_prefix(1);
        return parse_result::success<char>{str, c};
      } else {
        return parse_result::not_matched{str, "expected particular character"};
      }
    }
  };
  template<class F>
  char_predicate(F) -> char_predicate<F>;
  constexpr auto whitespace = char_predicate{[](char c){ return std::isspace(c); }};
  constexpr auto alphanumeric = char_predicate{[](char c){ return std::isalnum(c); }};
  constexpr auto alpha = char_predicate{[](char c){ return std::isalpha(c); }};
  constexpr auto digit = char_predicate{[](char c){ return std::isdigit(c); }};
  constexpr auto id_start = char_predicate{[](char c){ return c == '_' || std::isalpha(c); }};
  constexpr auto id_tail = char_predicate{[](char c){ return c == '_' || std::isalnum(c); }};

  template<parser P, class R = parser_return_type<P> >
  parsing_routine<std::vector<R> > repeat(P&& parser) {
    std::vector<R> ret;
    while(auto next = co_await optional{parser}) {
      ret.push_back(std::move(*next));
    }
    co_return ret;
  }
  template<parser P, parser T, class R = parser_return_type<T> >
  parsing_routine<R> preceded(P&& ignored, T&& data) {
    co_await std::forward<P>(ignored);
    co_return co_await std::forward<T>(data);
  }

  inline parsing_routine<std::monostate> parse_identifier_recognizer() {
    co_await error_override{"expected identifier"};
    co_await id_start;
    co_await repeat(id_tail);
    co_return std::monostate{};
  }
  constexpr auto parse_identifier = [](std::string_view str){
    return fmap{capture{parse_identifier_recognizer()}, [](auto pair){ return pair.second; }}(str);
  };
  constexpr auto whitespaces = [](std::string_view str){
    return repeat(whitespace)(str);
  };
  template<class T>
  inline parsing_routine<T> parse_uint_routine() {
    co_await error_override{"expected integer literal"};
    T ret = 0;
    auto first = int(co_await digit - '0');
    ret += first;
    while(auto next = co_await optional{digit}) {
      ret *= 10;
      ret += int(*next - '0');
    }
    co_return ret;
  }
  template<class T>
  constexpr auto parse_uint = [](std::string_view str){
    return parse_uint_routine<T>()(str);
  };
}
