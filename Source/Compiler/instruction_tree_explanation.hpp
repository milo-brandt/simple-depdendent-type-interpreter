#ifndef INSTRUCTION_TREE_EXPLANATION
#define INSTRUCTION_TREE_EXPLANATION

#include "../ExpressionParser/parser_tree.hpp"

namespace compiler::instruction {
  using Explanation = int;//expression_parser::archive_index::PolymorphicKind;
  enum class Primitive {
    type, arrow
  };
  inline std::ostream& operator<<(std::ostream& o, Primitive primitive) {
    switch(primitive) {
      case Primitive::type: return o << "type";
      case Primitive::arrow: return o << "arrow";
      default: return o << "(invalid primitive)";
    }
  }
}

#endif
