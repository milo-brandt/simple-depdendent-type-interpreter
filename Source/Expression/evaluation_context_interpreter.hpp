#ifndef EVALUATION_CONTEXT_INTERPRETER_HPP
#define EVALUATION_CONTEXT_INTERPRETER_HPP

#include "evaluation_context.hpp"
#include "../Compiler/flat_instructions.hpp"
#include "solver.hpp"

namespace expression {
  struct EmbedInfo {
    std::vector<TypedValue> values;
  };
  struct InterpretResult {
    std::vector<solve::ConstraintSpecification> constraints;
    std::vector<solve::CastSpecification> casts;
    std::vector<std::uint64_t> holes;
    TypedValue result;
  };
  InterpretResult interpret_program(compiler::flat::instruction::Program const& program, Context& context, EmbedInfo const& embed);
}

#endif
