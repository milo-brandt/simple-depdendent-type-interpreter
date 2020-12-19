#include "expression_parser.hpp"
#include "operator_tree_parser.hpp"

struct times_plus_neg_expr_tree {
  using return_type = operator_tree::tree<binary_operator, lambda_abstraction>;
  using unary_t = lambda_abstraction;
  dynamic_parser<lambda_abstraction> left_unary_op = surrounded(recognizer("\\"),sequence{parse_identifier, optional{surrounded{sequence{*whitespace, recognizer(":"), *whitespace}, expression_parser() ,*whitespace}}}, recognizer("."))
    > [](auto id){ auto& [arg_name, arg_type] = id; return lambda_abstraction{arg_name, arg_type ? *arg_type : nullptr}; };
  dynamic_parser<lambda_abstraction> right_unary_op = fail<lambda_abstraction>;
  dynamic_parser<binary_operator> binary_op = either{
    recognizer(":") > [](auto){ return binary_operator::is_a; },
    recognizer("->") > [](auto){ return binary_operator::arrow; },
    recognizer("=") > [](auto){ return binary_operator::equal; }
  } > utility::collapse_variant_legs;
  std::optional<binary_operator> superposition_op = binary_operator::superposition;
  auto embed_op(auto x) const { return x; }
  auto associate_left(lambda_abstraction x, binary_operator y) const {
    return false;
  }
  auto associate_left(binary_operator x, binary_operator y) const {
    if((int)x < (int)y) return true;
    if(x == y && x != binary_operator::arrow) return true;
    return false;
  }
  bool associate_left(auto x, lambda_abstraction y) const {
    std::terminate(); //handles right unary operators (there are none)
  }
  parse_result<return_type> parse(std::string_view str) const {
    return build_parser(*this).parse(str);
  }
};
dynamic_parser<operator_tree::tree<binary_operator, lambda_abstraction> > expression_parser() {
  return lazy{[](){ return times_plus_neg_expr_tree{}; }};
}
