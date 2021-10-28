#ifndef ARENA_MODULE_CORE_DEBUG_PRINT_HPP
#define ARENA_MODULE_CORE_DEBUG_PRINT_HPP

#include "core.hpp"
#include <iostream>

namespace expr_module {
  /*
    Prints the core as a valid (up to namespaces) C++ expression.
  */
  struct DebugCoreFormat {
    Core const& core;
  };
  inline DebugCoreFormat debug_format(Core const& core) { return {core}; }
  std::ostream& operator<<(std::ostream&, DebugCoreFormat);
}

#endif
