#include <catch.hpp>
#include "../expression_folding_utility.hpp"
#include "../parser_utility.hpp"

struct basic_folder {
  using term_t = int;
  enum binop_t {
    add,
    times,
    exp
  };
  enum left_unop_t {
    neg
  };
  enum right_unop_t {
    factorial
  };
  using fold_result = int;
  fold_result process_term(term_t term) {
    return term;
  }
  fold_result process_binary(fold_result lhs, binop_t op, fold_result rhs) {
    if(op == add) {
      return lhs + rhs;
    } else if (op == times) {
      return lhs * rhs;
    } else {
      int ret = 1;
      for(int i = 0; i < rhs; ++i) {
        ret *= lhs;
      }
      return ret;
    }
  }
  fold_result process_left_unary(left_unop_t op, fold_result rhs) {
    return -rhs;
  }
  fold_result process_right_unary(fold_result lhs, right_unop_t op) {
    int ret = 1;
    for(int i = 1; i <= lhs; ++i) ret *= i;
    return ret;
  }
  bool associate_left(binop_t lhs, binop_t rhs) {
    if(rhs == exp) return false;
    if(lhs == exp) return true;
    if(lhs == times) return true;
    if(rhs == times) return false;
    return true;
  }
  bool associate_left(binop_t lhs, right_unop_t rhs) {
    return false;
  }
  bool associate_left(left_unop_t, binop_t rhs) {
    return true;
  }
  bool associate_left(left_unop_t, right_unop_t) {
    return false;
  }
};

parse::parsing_routine<folder_input_vector<basic_folder> > parse_expression() {
  using namespace parse;
  bool is_expression = false;
  folder_input_vector<basic_folder> ret;
  while(true) {
    co_await whitespaces;
    if(is_expression) {
      if(co_await optional{tag{"+"}}){ is_expression = false; ret.push_back(basic_folder::add); }
      else if(co_await optional{tag{"*"}}){ is_expression = false; ret.push_back(basic_folder::times); }
      else if(co_await optional{tag{"^"}}){ is_expression = false; ret.push_back(basic_folder::exp); }
      else if(co_await optional{tag{"!"}}){ is_expression = true; ret.push_back(basic_folder::factorial); }
      else co_return ret;
    } else {
      if(co_await optional{tag{"-"}}){ is_expression = false; ret.push_back(basic_folder::neg); }
      auto term = co_await parse_uint<int>;
      ret.push_back(term);
      is_expression = true;
    }
  }
}

TEST_CASE("Parsed expressions are interpreted accurately") {
  auto check_case = [&](std::string_view input, int output) {
    auto x = parse_expression()(input);
    REQUIRE(x.index() == 2);
    REQUIRE(fold_flat_expression(basic_folder{}, std::get<2>(x).value) == output);
  };
  check_case("1+2",3);
  check_case("1+3!",7);
  check_case("-2!!!!!+-3!!",-722);
  check_case("2^3",8); //non-commutative
  check_case("2^3^2",512); //non-associative
}
