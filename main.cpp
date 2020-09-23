//#include "Runtime/builder.hpp"
#include <iostream>
#include "Runtime/runtime.hpp"
#include <chrono>

//using namespace runtime;
//using namespace runtime::instructions;
using namespace runtime;
using namespace runtime::instructions;
using namespace runtime::instructions::body;
template<class F>
decltype(auto) time_function(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  decltype(auto) ret = std::forward<F>(func)();
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "Took " << duration.count() << " microseconds.\n";
  return ret;
}
int main() {
  block fibonacci {
    .n_args = 1,
    .instructions = {
      constant {0}, //1
      constant {1} //2
    },
    .end = terminator::jump{0, {0, 1, 2}},
    .children = {
      block{
        .n_args = 3, //3, 4, 5
        .instructions = {
          add {4, 5}, //6
          constant {1}, //7
          sub {3, 7} //8
        },
        .end = terminator::br {
          .condition = 3,
          .if_true = {0, {8, 6, 4}},
          .if_false = {1, {4}}
        }
      },
      block{
        .n_args = 1, //3
        .instructions = {},
        .end = terminator::ret{3}
      }
    }
  };
  std::size_t cur = 0;
  std::size_t prev = 1;
  for(std::size_t i = 0; i < 80; ++i) {
    std::size_t passed_args[] = {i};
    std::cout << i << ": got " << fibonacci.simulate(passed_args) << " expected " << cur << "\n";
    std::swap(cur, prev);
    cur += prev;
  }
  environment e;
  auto f = e.compile(fibonacci);
  std::cout << time_function([&](){ return f(1000000000); }) << "\n";
  std::size_t passed_args[] = {1000000000};
  std::cout << time_function([&](){ return fibonacci.simulate(passed_args); }) << "\n";
}
