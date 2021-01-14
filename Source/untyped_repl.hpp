#ifndef UNTYPED_REPL_HPP
#define UNTYPED_REPL_HPP

#include "parser_utility.hpp"
#include "standard_expression.hpp"
#include "standard_expression_parser.hpp"
#include "standard_expression_compiler.hpp"

namespace untyped_repl {
  struct full_context {
    standard_compiler::name_context names;
    standard_compiler::context expr;
    std::ostream* output = nullptr;
  };
  struct parsed_rule {
    standard_parser::folded_parser_output::any pattern;
    standard_parser::folded_parser_output::any replacement;
    void execute(full_context&);
  };
  struct parsed_declaration {
    std::string_view name;
    void execute(full_context&);
  };
  struct parsed_axiom {
    std::string_view name;
    void execute(full_context&);
  };
  struct parsed_evaluator {
    standard_parser::folded_parser_output::any expr;
    std::optional<std::string_view> set_name;
    void execute(full_context&);
  };
  struct parsed_test {
    standard_parser::folded_parser_output::any expr;
    standard_parser::folded_parser_output::any result;
    std::optional<std::string> execute(full_context&);
  };
  using parsed_command = std::variant<parsed_rule, parsed_axiom, parsed_declaration, parsed_evaluator, parsed_test>;
  parse::parsing_routine<parsed_command> parse_command();
  parse::parsing_routine<parsed_command> parse_terminated_command();
  std::string load_file(std::string path);
}

#endif
