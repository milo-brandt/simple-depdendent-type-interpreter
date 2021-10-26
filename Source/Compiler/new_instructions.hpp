#ifndef COMPILER_NEW_INSTRUCTIONS_HPP
#define COMPILER_NEW_INSTRUCTIONS_HPP

#include "new_instruction_tree_impl.hpp"
#include "../Utility/result.hpp"

namespace compiler::new_instruction {
  struct InstructionContext {
    std::size_t empty_vec_embed_index = (std::size_t)-1;
    std::size_t cons_vec_embed_index = (std::size_t)-1;
  };
  located_output::Program make_instructions(expression_parser::resolved::archive_part::Expression const& expression, InstructionContext const& context = {});
}

#endif
