#ifndef RESOLVED_TREE_HPP
#define RESOLVED_TREE_HPP

#include "resolved_tree_impl.hpp"
#include "../ExpressionParser/parser_tree.hpp"
#include "../Utility/result.hpp"
#include <unordered_map>

namespace compiler::resolution {
  struct NameInfo {
    std::uint64_t embed_index;
    std::uint64_t hidden_args;
  };
  struct NameContext {
    std::unordered_map<std::string, NameInfo> names;
  };
  struct Input {
    expression_parser::output::Tree const& tree;
    NameContext const& context;
    std::uint64_t embed_arrow;
  };
  struct Output {
    located_output::Tree tree;
  };
  namespace error {
    struct UnrecognizedId {
      expression_parser::path::Path where;
    };
    using Any = std::variant<UnrecognizedId>;
  };
  mdb::Result<Output, error::Any> resolve(Input);
}

#endif
