#ifndef COMPILER_INSTRUCTIONS_HPP
#define COMPILER_INSTRUCTIONS_HPP

#include "new_instruction_tree_impl.hpp"
#include "../Utility/result.hpp"

namespace compiler::new_instruction {
  located_output::Program make_instructions(expression_parser::resolved::archive_part::Expression const& expression);
}

#endif
