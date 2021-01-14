#include <iostream>
#include <fstream>

#include "untyped_repl.hpp"
using namespace untyped_repl;
using standard_expression = expressionz::standard::standard_expression;
using shared_string = expressionz::standard::primitives::shared_string;

int main() {
  full_context ctx;
  ctx.output = &std::cout;
  try {
    auto preamble = load_file("source.txt");
    std::string_view remaining = preamble;
    while(true) {
      auto result = parse_terminated_command()(remaining);
      if(auto* err = std::get_if<0>(&result)) {
        std::cout << "Parsed until " << err->position << "\n";
        std::cout << "Error: " << err->reason << "\n";
        for(auto& [frame, pos] : err->stack) {
          std::cout << "While parsing: " << frame << "\n";
          std::cout << "At: " << pos << "\n";
        }
        break;
      } else if(auto* err = std::get_if<1>(&result)) {
        std::cout << "Parsed until " << err->position << "\n";
        std::cout << "Failure: " << err->reason << "\n";
        for(auto& [frame, pos] : err->stack) {
          std::cout << "While parsing: " << frame << "\n";
          std::cout << "At: " << pos << "\n";
        }
        break;
      } else {
        auto& success = std::get<2>(result);
        std::visit([&](auto&& x){ std::move(x).execute(ctx); }, std::move(success.value));
        remaining = success.position;
      }
    }
  } catch(std::runtime_error& e) {
    std::cout << e.what() << "\n";
  }
  auto import_from_c = [&](std::string name, auto&& f) {
    ctx.names.names.insert(std::make_pair(name, (expressionz::standard::standard_expression)ctx.expr.import_c_function(name, utility::to_raw_function(f))));
  };
  //defining u64_induct does not work well
  import_from_c("u64_add", [](std::uint64_t a, std::uint64_t b){ return a + b; });
  import_from_c("u64_sub", [](std::uint64_t a, std::uint64_t b){ return a - b; });
  import_from_c("u64_mul", [](std::uint64_t a, std::uint64_t b){ return a * b; });
  import_from_c("u64_if", [](std::uint64_t c) -> standard_expression { using namespace expressionz; return abstract((standard_expression)abstract(argument(c ? 1 : 0))); });
  import_from_c("cat", [](shared_string a, shared_string b){ return shared_string{std::string(a.data) + std::string(b.data)}; });
  while(true){
    std::string input;
    std::getline(std::cin, input);
    if(input == "quit") return 0;
    auto result = parse_command()(input);
    if(auto* err = std::get_if<0>(&result)) {
      std::cout << "Parsed until " << err->position << "\n";
      std::cout << "Error: " << err->reason << "\n";
      for(auto& [frame, pos] : err->stack) {
        std::cout << "While parsing: " << frame << "\n";
        std::cout << "At: " << pos << "\n";
      }
    } else if(auto* err = std::get_if<1>(&result)) {
      std::cout << "Parsed until " << err->position << "\n";
      std::cout << "Failure: " << err->reason << "\n";
      for(auto& [frame, pos] : err->stack) {
        std::cout << "While parsing: " << frame << "\n";
        std::cout << "At: " << pos << "\n";
      }
    } else {
      std::visit([&](auto&& x){ std::move(x).execute(ctx); }, std::move(std::get<2>(result).value));
    }

  }
}
