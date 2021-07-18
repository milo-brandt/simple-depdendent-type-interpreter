#include "WebInterface/web_interface.hpp"
#include "ExpressionParser/parser_tree.hpp"

#include "ExpressionParser/expression_parser.hpp"

#include <iostream>
#include "Utility/overloaded.hpp"
#include "Compiler/resolved_tree.hpp"
#include "Compiler/flat_instructions.hpp"

int main(int argc, char** argv) {

  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line == "q") return 0;

    std::string_view source = line;

    auto x = expression_parser::parser::expression(source);

    if(auto* error = parser::get_if_error(&x)) {
      std::cout << "Error: " << error->error << "\nAt char: " << (error->position.begin() - (std::string_view(source)).begin()) << "\n";
    } else {
      auto& success = parser::get_success(x);
      std::cout << "Parse result: ";
      format_indented(std::cout, success.value.locator, 0, [](auto& o, auto const& v) { o << v; });
      std::cout << "\n";

      compiler::resolution::NameContext context;
      context.names.insert_or_assign("sev", compiler::resolution::NameInfo{17});
      auto resolve = compiler::resolution::resolve({
        .tree = success.value.output,
        .context = context,
        .embed_arrow = 99
      });

      if(auto* err = resolve.get_if_error()) {
        std::cout << "Error!\n";
      } else {
        auto& resolved = resolve.get_value();
        std::cout << "Resolve result: ";
        format_indented(std::cout, resolved.tree.output, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";

        auto [program, explain] = compiler::flat::flatten_tree(resolved.tree.output);
        std::cout << "Program:\n" << program << "\n";
      }
    }
  }
}
