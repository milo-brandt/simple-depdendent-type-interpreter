#include <iostream>
#include "raw_helper.hpp"

std::size_t add(std::size_t a, std::size_t b){
  return a + b;
}

int main(){
  using namespace type_theory::raw;

  auto builtin = helper::convert_function_to_builtin(&add);

  auto doubler = make_apply(make_apply(builtin, make_placeholder(0)), make_placeholder(0));
  auto quadrupler = make_apply(make_apply(builtin, doubler), doubler);
  std::cout << quadrupler << "\n";
  auto pair_maker_f = helper::convert_placeholder_to_lambda(quadrupler, 0);
  std::cout << pair_maker_f << "\n";
  std::cout << apply(pair_maker_f, make_placeholder(0)) << "\n";
  return 0;
}
