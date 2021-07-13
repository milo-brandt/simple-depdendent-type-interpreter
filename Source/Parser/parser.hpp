#ifndef PARSER_PARSER_HPP
#define PARSER_PARSER_HPP

#include <string>
#include <variant>
#include <ctype.h>
#include <cstring>
#include <type_traits>
#include <vector>
#include <tuple>
#include <optional>

namespace parser {
  struct Empty{};
  constexpr Empty empty{};
  struct ParseError {
    std::string error;
    std::string_view position;
  };
  template<class T>
  struct ParseSuccess {
    T value;
    std::string_view remaining;
  };
  template<class T>
  ParseSuccess(T, std::string_view) -> ParseSuccess<T>;
  template<class T>
  using ParseResult = std::variant<ParseSuccess<T>, ParseError>;


  /*
  Accessors
  */

  template<class T>
  bool holds_success(ParseResult<T> const& result) {
    return result.index() == 0;
  }
  template<class T>
  bool holds_failure(ParseResult<T> const& result) {
    return result.index() == 1;
  }
  template<class T>
  ParseSuccess<T>& get_success(ParseResult<T>& result) {
    return std::get<0>(result);
  }
  template<class T>
  ParseError& get_error(ParseResult<T>& result) {
    return std::get<1>(result);
  }
  template<class T>
  ParseSuccess<T> const& get_success(ParseResult<T> const& result) {
    return std::get<0>(result);
  }
  template<class T>
  ParseError const& get_error(ParseResult<T> const& result) {
    return std::get<1>(result);
  }
  template<class T>
  ParseSuccess<T>* get_if_success(ParseResult<T>* result) {
    return std::get_if<0>(result);
  }
  template<class T>
  ParseError* get_if_error(ParseResult<T>* result) {
    return std::get_if<1>(result);
  }
  template<class T>
  ParseSuccess<T> const* get_if_success(ParseResult<T> const* result) {
    return std::get_if<0>(result);
  }
  template<class T>
  ParseError const* get_if_error(ParseResult<T> const* result) {
    return std::get_if<1>(result);
  }

  /*
    Combinators
  */

  namespace detail {
    template<class T>
    struct ParseResultExtractor;
    template<class T>
    struct ParseResultExtractor<ParseResult<T> > {
      using Type = T;
    };
  }
  template<class T>
  using ParserTypeOf = typename detail::ParseResultExtractor<std::decay_t<std::invoke_result_t<T, std::string_view> > >::Type;

  template<class T>
  constexpr auto char_predicate(T predicate) {
    return [predicate](std::string_view str) -> ParseResult<char> {
      if(str.empty()) {
        return ParseError{"Character expected, found EOF.", str};
      } else if(predicate(str[0])) {
        return ParseSuccess{str[0], str.substr(1)};
      } else {
        return ParseError{"Character failed predicate.", str.substr(0, 1)};
      }
    };
  }

  constexpr auto symbol(const char* sym) {
    std::size_t sym_len = 0;
    while(*(sym + sym_len)) ++sym_len;
    return [sym, size = sym_len](std::string_view str) -> ParseResult<Empty> {
      if(str.starts_with(sym)) {
        return ParseSuccess{empty, str.substr(size)};
      } else {
        return ParseError{std::string{"Expected "} + sym, str.substr(0, size)};
      }
    };
  }
  template<class T>
  constexpr auto integer = [](std::string_view str) -> ParseResult<T> {
    if(!str.empty() && '0' <= str[0] && str[0] <= '9') {
      T ret = 0;
      while(!str.empty() && '0' <= str[0] && str[0] <= '9') {
        T digit = str[0] - '0';
        ret *= 10;
        ret += digit;
        str.remove_prefix(1);
      }
      return ParseSuccess{std::move(ret), str};
    } else {
      return ParseError{"Expected digit.", str.substr(0, 1)};
    }
  };
  constexpr auto always_match = [](std::string_view str) -> ParseResult<Empty> {
    return ParseSuccess{empty, str};
  };
  namespace detail {
    template<class Call, class First, class Second, class... Args> decltype(auto) unpack_call(Call&& call, std::pair<First, Second>&& pair, Args&&... args);
    template<class Call, class Arg, class... Args> decltype(auto) unpack_call(Call&& call, Arg&& arg, Args&&... args);
    template<class Call> decltype(auto) unpack_call(Call&& call);
    template<class Call, class... TupleArgs, class... Args>
    decltype(auto) unpack_call(Call&& call, std::tuple<TupleArgs...>&& tuple, Args&&... args) {
      return std::apply([&](TupleArgs&&... tuple_args) {
        return unpack_call(std::forward<Call>(call), std::forward<TupleArgs>(tuple_args)..., std::forward<Args>(args)...);
      }, std::move(tuple));
    }
    template<class Call, class First, class Second, class... Args>
    decltype(auto) unpack_call(Call&& call, std::pair<First, Second>&& pair, Args&&... args) {
      return unpack_call(call, std::forward<First>(pair.first), std::forward<Second>(pair.second), std::forward<Args>(args)...);
    }
    template<class Call, class Arg, class... Args>
    decltype(auto) unpack_call(Call&& call, Arg&& arg, Args&&... args) {
      return unpack_call([&]<class... Inner>(Inner&&... inner) {
        return call(std::forward<Arg>(arg), std::forward<Inner>(inner)...);
      }, std::forward<Args>(args)...);
    }
    template<class Call>
    decltype(auto) unpack_call(Call&& call) {
      return call();
    }
  }
  template<class F>
  constexpr auto unpacked(F map) {
    return [map]<class... Args>(Args&&... args) -> decltype(auto) {
      return detail::unpack_call(map, std::forward<Args>(args)...);
    };
  }
  template<class Parser>
  constexpr auto no_result(Parser parser) {
    return [parser](std::string_view str) -> ParseResult<Empty> {
      auto ret = parser(str);
      if(auto* success = get_if_success(&ret)) {
        return ParseSuccess{empty, success->remaining};
      } else {
        return std::move(get_error(ret));
      }
    };
  }
  template<class Parser>
  constexpr auto captured(Parser parser) {
    using Type = ParserTypeOf<Parser>;
    return [parser](std::string_view str) -> ParseResult<std::pair<Type, std::string_view> > {
      auto r = parser(str);
      if(auto* success = get_if_success(&r)) {
        std::string_view range{str.begin(), success->remaining.begin()};
        return ParseSuccess{std::make_pair(std::move(success->value), range), success->remaining};
      } else {
        return std::move(get_error(r));
      }
    };
  }
  template<class Parser, class F>
  constexpr auto map(Parser parse, F map) {
    using ParserType = ParserTypeOf<Parser>;
    using Type = std::invoke_result_t<F, ParserType>;
    return [parse, map](std::string_view str) -> ParseResult<Type> {
      auto r = parse(str);
      if(auto* success = get_if_success(&r)) {
        return ParseSuccess{map(std::move(success->value)), success->remaining};
      } else {
        return std::move(get_error(r));
      }
    };
  }
  template<class Parser, class F>
  constexpr auto bind(Parser parse, F map) {
    using ParserType = ParserTypeOf<Parser>;
    using Type = ParserTypeOf<std::invoke_result_t<F, ParserType> >;
    return [parse, map](std::string_view str) -> ParseResult<Type> {
      auto r = parse(str);
      if(auto* success = get_if_success(&r)) {
        return map(std::move(success->value))(success->remaining);
      } else {
        return std::move(get_error(r));
      }
    };
  }
  template<class Parser, class F>
  constexpr auto bind_fold(Parser parse, F map) { //applies bind repeatedly
    using ParserType = ParserTypeOf<Parser>;
    using Type = ParserTypeOf<std::invoke_result_t<F, ParserType&> >;
    static_assert(std::is_same_v<ParserType, Type>, "Bind fold requires the base parser and bind parser to have same return type.");
    return [parse, map](std::string_view str) -> ParseResult<Type> {
      auto r = parse(str);
      if(auto* success = get_if_success(&r)) {
        ParseSuccess<Type> ret = std::move(*success);
        while(true) {
          auto next = map(ret.value)(ret.remaining);
          if(holds_success(next)) {
            ret = std::move(get_success(next));
          } else {
            break;
          }
        }
        return ret;
      } else {
        return std::move(get_error(r));
      }
    };
  }

  template<class Checker, class TrueBranch, class FalseBranch>
  constexpr auto condition_branch(Checker condition, TrueBranch true_branch, FalseBranch false_branch) {
    static_assert(std::is_same_v<ParserTypeOf<TrueBranch>, ParserTypeOf<FalseBranch> >, "Branches must have same type.");
    return [condition, true_branch, false_branch](std::string_view str) {
      if(holds_success(condition(str))) {
        return true_branch(str);
      } else {
        return false_branch(str);
      }
    };
  }

  namespace detail {
    template<class T>
    ParseResult<T> parse_branch(std::string_view str) {
      return ParseError{"No branches found to handle string.", str.substr(0, 0)};
    }
    template<class T, class Condition, class Action, class... Rest>
    ParseResult<T> parse_branch(std::string_view str, std::pair<Condition, Action> const& head, Rest const&... rest) {
      if(holds_success(head.first(str))) {
        return head.second(str);
      } else {
        return parse_branch<T>(str, rest...);
      }
    }
  }

  template<class Condition, class Action, class... Conditions, class... Actions>
  constexpr auto branch(std::pair<Condition, Action> first, std::pair<Conditions, Actions>... args) {
    static_assert((std::is_same_v<
      ParserTypeOf<Action>,
      ParserTypeOf<Actions>
    > && ...), "All parsers must return same type in branch.");
    using Type = ParserTypeOf<Action>;
    return [choices = std::make_tuple(first, args...)](std::string_view str) -> ParseResult<Type> {
      return std::apply([&](auto&&... choices) {
        return detail::parse_branch<Type>(
          str,
          choices...
        );
      }, choices);
    };
  }
  template<class... Conditions, class... Actions>
  constexpr auto branch_after(std::pair<Conditions, Actions>... args) { //avoid repeating branch statement
    return branch(
      std::make_pair(args.first, sequence(no_result(args.first), std::move(args.second)))...
    );
  }
  template<class Parser>
  constexpr auto zero_or_more(Parser parse) {
    using Type = ParserTypeOf<Parser>;
    return [parse](std::string_view str) -> ParseResult<std::vector<Type> > {
      std::vector<Type> vec;
      while(true) {
        auto ret = parse(str);
        if(auto* success = get_if_success(&ret)) {
          str = success->remaining;
          vec.push_back(std::move(success->value));
        } else {
          return ParseSuccess{std::move(vec), str};
        }
      }
    };
  }
  template<class Parser>
  constexpr auto one_or_more(Parser parse) {
    using Type = ParserTypeOf<Parser>;
    return [parse](std::string_view str) -> ParseResult<std::vector<Type> > {
      std::vector<Type> vec;
      while(true) {
        auto ret = parse(str);
        if(auto* success = get_if_success(&ret)) {
          str = success->remaining;
          vec.push_back(std::move(success->value));
        } else {
          if(vec.empty()) {
            return get_error(ret);
          } else {
            return ParseSuccess{std::move(vec), str};
          }
        }
      }
    };
  }
  template<class Parser>
  constexpr auto optional(Parser parse) {
    using Type = ParserTypeOf<Parser>;
    return [parse](std::string_view str) -> ParseResult<std::optional<Type> > {
      auto ret = parse(str);
      if(auto* success = get_if_success(&ret)) {
        return ParseSuccess{std::optional<Type>{std::move(success->value)}, success->remaining};
      } else {
        return ParseSuccess{std::optional<Type>{}, str};
      }
    };
  }
  template<class Condition, class Parser>
  constexpr auto condition_optional(Condition condition, Parser parse) {
    using Type = ParserTypeOf<Parser>;
    return [condition, parse](std::string_view str) -> ParseResult<std::optional<Type> > {
      if(holds_success(condition(str))) {
        return parse(str);
      } else {
        return ParseSuccess{std::optional<Type>{}, str};
      }
    };
  }
  template<class Condition, class Parser>
  constexpr auto condition_after_optional(Condition condition, Parser parse) {
    return condition_optional(condition, sequence(no_result(condition), std::move(parse)));
  }

  namespace detail {
    template<class T, class Finisher>
    ParseResult<T> parse_sequence(std::string_view str, Finisher&& finisher) {
      return ParseSuccess<T>{std::forward<Finisher>(finisher)(), str};
    }
    template<class T, class Finisher, class Parser, class... Parsers>
    ParseResult<T> parse_sequence(std::string_view str, Finisher&& finisher, Parser&& parse, Parsers&&... parsers) {
      auto ret = std::forward<Parser>(parse)(str);
      if(auto* success = get_if_success(&ret)) {
        return parse_sequence<T>(
          success->remaining,
          [&]<class... Args>(Args&&... args) {
            if constexpr(std::is_same_v<decltype(success->value), Empty>) {
              return std::forward<Finisher>(finisher)(std::forward<Args>(args)...);
            } else {
              return std::forward<Finisher>(finisher)(std::move(success->value), std::forward<Args>(args)...);
            }
          },
          std::forward<Parsers>(parsers)...
        );
      } else {
        return std::move(get_error(ret));
      }
    }
    template<class... Ts>
    struct JoinType;
    template<>
    struct JoinType<std::tuple<> > {
      using Type = Empty;
    };
    template<class T1>
    struct JoinType<std::tuple<T1> > {
      using Type = T1;
    };
    template<class T1, class T2, class... Ts>
    struct JoinType<std::tuple<T1, T2, Ts...> > {
      using Type = std::tuple<T1, T2, Ts...>;
    };
    template<class... Ts, class... Rest>
    struct JoinType<std::tuple<Ts...>, Empty, Rest...> : JoinType<std::tuple<Ts...>, Rest...> {};
    template<class... Ts, class T, class... Rest>
    struct JoinType<std::tuple<Ts...>, T, Rest...> : JoinType<std::tuple<Ts..., T>, Rest...> {};
  }
  template<class... Parsers>
  constexpr auto sequence(Parsers... parsers) {
    using Type = typename detail::JoinType<std::tuple<>, ParserTypeOf<Parsers>...>::Type;
    return [parser_tuple = std::make_tuple(parsers...)](std::string_view str) -> ParseResult<Type> {
      return std::apply([&](auto&&... parsers) {
        return detail::parse_sequence<Type>(
          str,
          []<class... Args>(Args&&... args) {
            if constexpr(sizeof...(Args) > 1) {
              return std::make_tuple(std::forward<Args>(args)...);
            } else if constexpr(sizeof...(Args) == 1) {
              return (args , ...);
            } else {
              return empty;
            }
          },
          parsers...
        );
      }, parser_tuple);
    };
  }

  constexpr auto whitespace_char = char_predicate([](char c){ return std::isspace(c); });
  constexpr auto maybe_whitespace = no_result(zero_or_more(whitespace_char));
  constexpr auto whitespace = no_result(one_or_more(whitespace_char));

  namespace detail {
    template<class Finisher>
    constexpr auto build_loose_sequence(Finisher&& finisher) {
      return finisher();
    }
    template<class Finisher, class Parser, class... Parsers>
    constexpr auto build_loose_sequence(Finisher&& finisher, Parser&& parser, Parsers&&... parsers) {
      return build_loose_sequence([&]<class... Args>(Args&&... args) {
        return finisher(maybe_whitespace, std::forward<Parser>(parser), std::forward<Args>(args)...);
      }, std::forward<Parsers>(parsers)...);
    }
  }
  template<class Parser, class... Parsers> //interleaves maybe_whitespace in between everything
  constexpr auto loose_sequence(Parser parser, Parsers... parsers) {
    return detail::build_loose_sequence(
      [&parser]<class... Args>(Args&&... args) {
        return sequence(std::move(parser), std::forward<Args>(args)...);
      }, std::move(parsers)...
    );
  }
  constexpr auto loose_symbol(const char* sym) {
    return sequence(whitespace, symbol(sym));
  }
  template<class Parser>
  constexpr auto loose_capture(Parser parser) {
    return sequence(whitespace, capture(parser));
  }

  /*

  */


/*
  An expression is...
    (var_name : expression) -> expression
    expression -> expression
    term term term ... term

  A term is...
    (expression)
    \var_name.expression
    \var_name:expression.expression


*/
}

#endif
