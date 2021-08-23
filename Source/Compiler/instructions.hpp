#ifndef COMPILER_INSTRUCTIONS_HPP
#define COMPILER_INSTRUCTIONS_HPP

#include "instruction_context_impl.hpp"
#include "instruction_tree_impl.hpp"
#include "../Utility/result.hpp"

namespace compiler::instruction {
  struct Error {
    std::vector<std::pair<std::string_view, expression_parser::archive_index::PolymorphicKind> > unrecognized_ids;
  };
  mdb::Result<located_output::Program, Error> resolve(expression_parser::output::archive_part::Expression const& expression, Context global_context);
}

#endif
