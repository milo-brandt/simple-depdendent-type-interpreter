#ifndef PARSER_RECURSIVE_MACROS_HPP
#define PARSER_RECURSIVE_MACROS_HPP

#include "parser.hpp"

namespace parser {
  namespace detail {
    template<class T, class Ret, class Arg>
    struct HoldEvaluator {
      static ParseResult<Ret> evaluate(std::string_view str) {
        return T::Detail::value(str);
      }
    };
    template<class T, class Ret = typename T::Type>
    struct HoldExpression {
      template<class Arg>
      constexpr ParseResult<Ret> operator()(Arg arg) const {
        std::string_view str = std::forward<Arg>(arg);
        return HoldEvaluator<T, Ret, Arg>::evaluate(str);
      }
    };
  }
}

#define MB_DECLARE_RECURSIVE_PARSER(NAME, TYPE)                                  \
  struct NAME##MBParserDetail {                                                  \
    using Type = TYPE;                                                           \
    struct Detail;                                                               \
  };                                                                             \
  constexpr auto NAME = ::parser::detail::HoldExpression<NAME##MBParserDetail>{}

#define MB_DEFINE_RECURSIVE_PARSER(NAME, DEFINITION)                             \
  struct NAME##MBParserDetail::Detail {                                          \
    static constexpr auto value = DEFINITION;                                    \
  }


#endif
