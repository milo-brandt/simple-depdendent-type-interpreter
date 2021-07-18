#ifndef PARSER_TREE_HPP
#define PARSER_TREE_HPP

#include "parser_tree_impl.hpp"

namespace expression_parser::locator {
  inline std::string_view position_of(Tree const& tree) {
    return tree.visit([](auto const& x) { return x.position; });
  }
}

#endif
