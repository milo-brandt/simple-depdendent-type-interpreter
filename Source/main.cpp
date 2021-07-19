//#include "WebInterface/web_interface.hpp"
#include "ExpressionParser/parser_tree.hpp"

#include "ExpressionParser/expression_parser.hpp"

#include <iostream>
#include "Utility/overloaded.hpp"
#include "Compiler/resolved_tree.hpp"
#include "Compiler/flat_instructions.hpp"
#include "Expression/evaluation_context_interpreter.hpp"
#include <sstream>

int main(int argc, char** argv) {
  expression::Context context;
  compiler::resolution::NameContext name_context;
  expression::EmbedInfo embeds;

  auto add_name = [&](std::string name, expression::TypedValue value) {
    name_context.names.insert_or_assign(std::move(name), compiler::resolution::NameInfo{
      .embed_index = embeds.values.size()
    });
    embeds.values.push_back(std::move(value));
  };

  add_name("Type", { expression::tree::External{context.primitives.type}, expression::tree::External{context.primitives.type} });
  add_name("arrow", { expression::tree::External{context.primitives.arrow}, context.primitives.arrow_type() });

  std::size_t counter = 0;
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

      auto resolve = compiler::resolution::resolve({
        .tree = success.value.output,
        .context = name_context,
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

        auto [ret_value, ret_type] = expression::interpret_program(program, context, embeds);

        for(auto const& rule : context.rules) {
          std::cout << "Pattern: ";
          format_indented(std::cout, rule.pattern, 0, [](auto& o, auto const& v) { o << v; });
          std::cout << "\n";
          std::cout << "Replacement: ";
          format_indented(std::cout, rule.replacement, 0, [](auto& o, auto const& v) { o << v; });
          std::cout << "\n\n";
        }

        std::cout << "Interpreted result: ";
        format_indented(std::cout, ret_value, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";
        std::cout << "Of type: ";
        format_indented(std::cout, ret_type, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";
        ret_value = context.reduce(std::move(ret_value));
        std::cout << "Simplified: ";
        format_indented(std::cout, ret_value, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";
        ret_type = context.reduce(std::move(ret_type));
        std::cout << "Of type: ";
        format_indented(std::cout, ret_type, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";

        std::stringstream str;
        str << "last" << counter++;
        add_name(str.str(), {std::move(ret_value), std::move(ret_type)});
      }
    }
  }
}
