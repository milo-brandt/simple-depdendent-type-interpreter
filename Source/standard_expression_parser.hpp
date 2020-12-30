#ifndef STANDARD_EXPRESSION_PARSER_HPP
#define STANDARD_EXPRESSION_PARSER_HPP

#include "parser_utility.hpp"
#include "standard_expression.hpp"
#include "indirect_variant.hpp"

namespace standard_parser {
  using standard_expression = expressionz::standard::standard_expression;
  namespace flat_parser_output {
    enum class binop : int {
      arrow,
      is_a,
      superposition,
      equals
    };
    struct left_unop {
      std::string_view arg_name;
    };
    enum right_unop{};
    struct term {
      using compound = std::vector<std::variant<term, binop, left_unop, right_unop> >;
      indirect_variant<std::string_view, compound> data;
    };
  }
  namespace folded_parser_output {
    struct id_t {
      std::string_view id;
    };
    struct binop;
    struct abstraction;
    using any = indirect_variant<id_t, binop, abstraction>;
    struct binop {
      flat_parser_output::binop index;
      any lhs;
      any rhs;
    };
    struct abstraction {
      std::string_view arg;
      any body;
    };
  };
  parse::parse_result::any<folded_parser_output::any> parse_expression(std::string_view input);
};

#endif
