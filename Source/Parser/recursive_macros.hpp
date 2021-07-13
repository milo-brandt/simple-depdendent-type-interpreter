#ifndef PARSER_RECURSIVE_MACROS_HPP
#define PARSER_RECURSIVE_MACROS_HPP

#include "parser.hpp"

namespace parser {
  template<class T>
  struct ReferenceWrapper {
    T& reference;
  };
  template<class T>
  ReferenceWrapper<T> ref(T& in) {
    return {in};
  }
  template<class T>
  ReferenceWrapper<T const> cref(T const& in) {
    return {in};
  }
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
    template<class T>
    struct ArgDecay {
      static T&& pass_through(T&& in) {
        return std::forward<T>(in);
      }
    };
    template<class T>
    struct ArgDecay<ReferenceWrapper<T> const& > {
      static T& pass_through(ReferenceWrapper<T> in) {
        return in.reference;
      }
    };
    template<class T, class Ret, class Arg, class... Args>
    struct ParametricHoldEvaluator {
      static ParseResult<Ret> evaluate(std::string_view str, Args&&... args) {
        return T::Detail::value(ArgDecay<Args>::pass_through(std::forward<Args>(args))...)(str);
      }
    };
    template<class T, class Ret = typename T::Type>
    struct ParametricHoldExpression {
      template<class... Args>
      constexpr auto operator()(Args&&... args) const {
        return [arg_tuple = std::make_tuple(std::forward<Args>(args)...)]<class Arg>(Arg arg) {
          std::string_view str = std::forward<Arg>(arg);
          return std::apply([&]<class... InnerArgs>(InnerArgs&&... inner_args) {
            return ParametricHoldEvaluator<T, Ret, Arg, InnerArgs&&...>::evaluate(str, std::forward<InnerArgs>(inner_args)...);
          }, arg_tuple);
        };
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

#define MB_DECLARE_PARAMETRIC_RECURSIVE_PARSER(NAME, TYPE)                       \
  struct NAME##MBParserDetail {                                                  \
    using Type = TYPE;                                                           \
    struct Detail;                                                               \
  };                                                                             \
  constexpr auto NAME = ::parser::detail::ParametricHoldExpression<NAME##MBParserDetail>{}

#define MB_DEFINE_PARAMETRIC_RECURSIVE_PARSER(NAME, DEFINITION)                  \
  struct NAME##MBParserDetail::Detail {                                          \
    static constexpr auto value = DEFINITION;                                    \
  }


#endif
