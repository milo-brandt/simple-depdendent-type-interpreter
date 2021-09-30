#ifndef COMPILER_REQUESTS_HPP
#define COMPILER_REQUESTS_HPP

#include "../Expression/expression_tree.hpp"
#include "../Utility/tags.hpp"
#include "../Expression/stack.hpp"

namespace compiler::request {
  struct Solve {
    static constexpr bool is_primitive = true;
    using RoutineType = mdb::Unit;
    expression::Stack stack;
    expression::tree::Expression lhs;
    expression::tree::Expression rhs;
  };
  struct DefineVariable {
    static constexpr bool is_primitive = true;
    using RoutineType = mdb::Unit;
    std::uint64_t head;
    std::uint64_t depth;
    expression::tree::Expression replacement;
  };
}

#endif
