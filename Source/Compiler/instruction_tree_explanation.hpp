#ifndef INSTRUCTION_TREE_EXPLANATION
#define INSTRUCTION_TREE_EXPLANATION

#include "../ExpressionParser/parser_tree.hpp"

namespace compiler::instruction {
  using Explanation = int;//expression_parser::archive_index::PolymorphicKind;
  enum class Primitive {
    type, arrow
  };
}

#endif
