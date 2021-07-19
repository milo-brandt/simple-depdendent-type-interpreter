#ifndef EVALUATION_CONTEXT_INTERPRETER_HPP
#define EVALUATION_CONTEXT_INTERPRETER_HPP

#include "evaluation_context.hpp"
#include "../Compiler/flat_instructions.hpp"

namespace expression {
  tree::Tree interpret_program(compiler::flat::instruction::Program const& program, Context& context);
}

#endif
