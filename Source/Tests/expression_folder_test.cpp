#include <catch.hpp>
#include <memory>
#include "../expression_folding_utility.hpp"

struct basic_folder {
  using term_t = int;
  enum binop_t {
    add,
    times
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
    } else {
      return lhs * rhs;
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
    return lhs == times || rhs == add;
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

TEST_CASE("Expression folder works correctly with basic folder") {
  folder_input_vector<basic_folder> input;
  input.push_back(basic_folder::neg);
  input.push_back(3);
  REQUIRE(fold_flat_expression(basic_folder{}, input) == -3);
  input.push_back(basic_folder::factorial);
  REQUIRE(fold_flat_expression(basic_folder{}, input) == -6);
  input.push_back(basic_folder::factorial);
  REQUIRE(fold_flat_expression(basic_folder{}, input) == -720);
  input.push_back(basic_folder::add);
  input.push_back(1);
  REQUIRE(fold_flat_expression(basic_folder{}, input) == -719);
  input.push_back(basic_folder::times);
  input.push_back(2);
  REQUIRE(fold_flat_expression(basic_folder{}, input) == -718);
}

struct basic_no_copy_folder {
  using term_t = std::unique_ptr<int>;
  using binop_t = std::unique_ptr<basic_folder::binop_t>;
  using left_unop_t = std::unique_ptr<basic_folder::left_unop_t>;
  using right_unop_t = std::unique_ptr<basic_folder::right_unop_t>;
  using fold_result = std::unique_ptr<int>;
  fold_result process_term(term_t term) {
    return term;
  }
  fold_result process_binary(fold_result lhs, binop_t op, fold_result rhs) {
    if(*op == basic_folder::add) {
      return std::make_unique<int>(*lhs + *rhs);
    } else {
      return std::make_unique<int>(*lhs * *rhs);
    }
  }
  fold_result process_left_unary(left_unop_t op, fold_result rhs) {
    return std::make_unique<int>(-*rhs);
  }
  fold_result process_right_unary(fold_result lhs, right_unop_t op) {
    int ret = 1;
    for(int i = 1; i <= *lhs; ++i) ret *= i;
    return std::make_unique<int>(ret);
  }
  bool associate_left(binop_t const& lhs, binop_t const& rhs) {
    return *lhs == basic_folder::times || *rhs == basic_folder::add;
  }
  bool associate_left(binop_t const& lhs, right_unop_t const& rhs) {
    return false;
  }
  bool associate_left(left_unop_t const&, binop_t const&) {
    return true;
  }
  bool associate_left(left_unop_t const&, right_unop_t const&) {
    return false;
  }
};

TEST_CASE("Expression folder works correctly with basic no copy folder") { //mostly a question of ensuring this compiles
  folder_input_vector<basic_no_copy_folder> input;
  input.push_back(std::make_unique<basic_folder::left_unop_t>(basic_folder::neg));
  input.push_back(std::make_unique<int>(3));
  REQUIRE(*fold_flat_expression(basic_no_copy_folder{}, std::move(input)) == -3);
}
