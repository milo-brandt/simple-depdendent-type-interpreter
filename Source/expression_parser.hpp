#ifndef EXPRESSION_PARSER_HPP
#define EXPRESSION_PARSER_HPP

#include "operator_tree.hpp"
#include "combinatorial_parser.hpp"

struct lambda_abstraction;
enum class binary_operator: int {
  superposition,
  equal,
  is_a,
  arrow
};
using parsed_expression_tree = operator_tree::tree<binary_operator, lambda_abstraction>;
struct lambda_abstraction {
  std::string_view arg_name;
  parsed_expression_tree arg_type = nullptr;
};
using parsed_binop = operator_tree::binary_operation<binary_operator, lambda_abstraction>;
using parsed_unop = operator_tree::unary_operation<binary_operator, lambda_abstraction>;
dynamic_parser<operator_tree::tree<binary_operator, lambda_abstraction> > expression_parser();

#endif
