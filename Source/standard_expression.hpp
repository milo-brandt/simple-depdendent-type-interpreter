#ifndef STANDARD_EXPRESSION_HPP
#define STANDARD_EXPRESSION_HPP

#include "expressions.hpp"
#include "expression_routine.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <cstdint>

namespace expressionz::standard {
  namespace primitives {
    struct axiom {
      std::size_t index;
      bool operator==(axiom const&) const = default;
    };
    struct declaration {
      std::size_t index;
      bool operator==(declaration const&) const = default;
    };
    struct shared_string {
      std::shared_ptr<std::string> handle;
      std::string_view data;
      shared_string(std::string str):handle(std::make_shared<std::string>(std::move(str))),data(*handle){}
      shared_string(std::shared_ptr<std::string> handle, std::string_view data):handle(std::move(handle)),data(data){}
      bool operator==(shared_string const& o) const { return o.data == data; }
    };
    using any = std::variant<axiom, declaration, std::uint64_t, shared_string>;
  };
  using standard_expression = expression<primitives::any>;
  template<class T>
  using routine = expressionz::coro::expression_routine<primitives::any, T>;
  template<class T, class... Args>
  using routine_generator = utility::fn_simple<routine<T>(Args...)>;
  struct simplification_result {
    primitives::any head; //will not be a declaration.
    std::vector<expressionz::coro::handles::expression> args;
  };
  using simplify_routine_t = routine_generator<std::optional<simplification_result>, expressionz::coro::handles::expression>;
  using simplification_rule_routine = routine_generator<std::optional<standard_expression>, simplify_routine_t const&, std::vector<expressionz::coro::handles::expression> >;
  struct axiom_info {
    std::string name;
  };
  struct simplification_rule {
    std::size_t args_consumed;
    simplification_rule_routine matcher;
    //simplification_rule from_pattern(standard_expression pattern, standard_expression result, context const&);
  };
  struct declaration_info {
    std::string name;
    std::vector<simplification_rule> rules;
  };
  struct context {
    std::vector<axiom_info> axioms;
    std::vector<declaration_info> declarations;
    primitives::axiom register_axiom(axiom_info);
    primitives::declaration register_declaration(declaration_info);
    void add_rule(primitives::declaration, simplification_rule);
    template<class Ret, class... Args> //creates a declaration
    primitives::declaration import_c_function(std::string name, Ret(*f)(Args...));
  };
  std::pair<primitives::declaration, simplification_rule> create_pattern_rule(standard_expression pattern, simplification_rule_routine result, context const&);
  std::pair<primitives::declaration, simplification_rule> create_pattern_replacement_rule(standard_expression pattern, standard_expression result, context const&);


  routine<std::optional<simplification_result> > simplify_outer(expressionz::coro::handles::expression, context const&);
  routine<std::monostate> simplify_total(expressionz::coro::handles::expression, context const&);
  routine<std::monostate> simple_output(expressionz::coro::handles::expression, context const&, std::ostream&);


  template<class Ret, class... Args> //creates a declaration
  primitives::declaration context::import_c_function(std::string name, Ret(*f)(Args...)) {
    static_assert(((std::is_same_v<Args, std::uint64_t> || std::is_same_v<Args, primitives::shared_string> || std::is_same_v<Args, standard_expression>)  && ...), "arguments can only be primitive types or standard_expression");
    static_assert(std::is_convertible_v<Ret, standard_expression>, "return type must be convertible to standard_expression");
    simplification_rule_routine impl = [&]<std::size_t... indices>(std::index_sequence<indices...>) {
      return [f](simplify_routine_t const& s, std::vector<expressionz::coro::handles::expression> args) -> routine<std::optional<standard_expression> > {
        using namespace expressionz::coro;
        if(args.size() != sizeof...(Args)) std::terminate(); //unreachable
        ((!std::is_same_v<Args, standard_expression> ? (void)(co_await s(args[indices])), 0 : 1) , ...);
        std::tuple<std::optional<Args>...> values = { //C++ at its finest - needed because we can't co_await in a lambda nor iterate over a parameter pack
          (std::is_same_v<Args, standard_expression> ?
            [&](auto&& v) -> std::optional<Args> {
              if constexpr(std::is_same_v<Args, standard_expression>) {
                return std::move(v);
              } else {
                std::terminate(); //unreachable
              }
            }(co_await actions::collapse(args[indices]))
          : //else
            [&](auto&& v) -> std::optional<Args> {
              if constexpr(std::is_same_v<Args, standard_expression>) {
                std::terminate(); //unreachable
              } else {
                if(auto* a = std::get_if<primitives::any>(&v)) {
                  if(auto* val = std::get_if<Args>(a)) {
                    return std::move(*val);
                  }
                }
                return std::nullopt;
              }
            }(co_await actions::expand(args[indices])))...
        };
        if(!((std::get<indices>(values)) && ...)) co_return std::nullopt;
        co_return (standard_expression)std::apply([&](auto&&... args){ return f(std::move(*args)...); }, std::move(values));
      };
    }(std::make_index_sequence<sizeof...(Args)>{});
    auto ret = register_declaration({std::move(name)});
    add_rule(ret, {sizeof...(Args), std::move(impl)});
    return ret;
  }
}

#endif
