#ifndef EXPRESSION_FORMATTER_HPP
#define EXPRESSION_FORMATTER_HPP

#include "expression_tree.hpp"
#include "evaluation_context.hpp"
#include <unordered_set>
#include <string>
#include <functional>
#include <sstream>
/*
  Future work - add explicit support for printing lambdas by
  recognizing lambdas in context and adding new terms to
  represent them (allowing for their rule to be simplified
  in an interactive manner)
*/

namespace expression::format {
  struct FormatContext {
    std::function<std::string(std::uint64_t)> format_external = [](std::uint64_t index) { std::stringstream str; str << "_" << index; return str.str(); };
    std::uint64_t arrow_external = -1;
    bool full_reduce = false;
  };
  struct FormatResult {
    std::string result;
    std::unordered_set<std::uint64_t> included_externals;
  };
  FormatResult format_expression(tree::Tree const&, Context& evaluation_context, FormatContext format_context);
  FormatResult format_pattern(pattern::Tree const&, Context& evaluation_context, FormatContext format_context);
}


#endif
