#ifndef COMPILER_INSTRUCTIONS_HPP
#define COMPILER_INSTRUCTIONS_HPP

#include "instruction_tree_impl.hpp"
#include "../Utility/result.hpp"

namespace compiler::instruction {
  located_output::Program make_instructions(expression_parser::resolved::archive_part::Expression const& expression);
}

#endif
