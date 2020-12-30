#ifndef STANDARD_EXPRESSION_COMPILER_HPP
#define STANDARD_EXPRESSION_COMPILER_HPP

#include "standard_expression_parser.hpp"
#include "result.hpp"
#include <unordered_set>
#include <unordered_map>

namespace standard_compiler {
  using namespace expressionz::standard;
  struct name_context {
    std::unordered_map<std::string, standard_expression> names;
  };
  std::unordered_set<std::string> find_unbound_names(standard_parser::folded_parser_output::any const&, name_context const&);
  result<standard_expression, std::string> compile(standard_parser::folded_parser_output::any const&, name_context const&);
}

#endif
