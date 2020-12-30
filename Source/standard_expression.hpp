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
    using any = std::variant<axiom, declaration, std::uint64_t>;
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
  };
  std::pair<primitives::declaration, simplification_rule> create_pattern_rule(standard_expression pattern, simplification_rule_routine result, context const&);
  std::pair<primitives::declaration, simplification_rule> create_pattern_replacement_rule(standard_expression pattern, standard_expression result, context const&);

  routine<std::optional<simplification_result> > simplify_outer(expressionz::coro::handles::expression, context const&);
  routine<std::monostate> simplify_total(expressionz::coro::handles::expression, context const&);
  routine<std::monostate> simple_output(expressionz::coro::handles::expression, context const&, std::ostream&);
}

#endif
