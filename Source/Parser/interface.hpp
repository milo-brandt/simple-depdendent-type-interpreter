#ifndef PARSER_INTERFACE_HPP
#define PARSER_INTERFACE_HPP

#include <optional>
#include "../Utility/shared_variant.hpp"

namespace parser {
  namespace tree {
    struct Apply;
    struct Identifier;
    struct Lambda;
    struct Arrow;
    using Node = mdb::SharedVariant<Apply, Identifier, Lambda, Arrow>;
    struct Apply {
      Node lhs;
      Node rhs;
    };
    struct Identifier {
      std::string_view name;
    };
    struct Lambda {
      std::optional<std::string_view> arg_name;
      std::optional<Node> type_declaration;
      Node body;
    };
    struct Arrow {
      std::optional<std::string_view> arg_name;
      Node domain;
      Node codomain;
    };
  };
  namespace tree_locator {
    struct Apply;
    struct Identifier;
    struct Lambda;
    struct Arrow;
    using Node = mdb::SharedVariant<Apply, Identifier, Lambda, Arrow>;
    struct Apply {
      std::string_view position;
      Node lhs;
      Node rhs;
    };
    struct Identifier {
      std::string_view position;
    };
    struct Lambda {
      std::string_view position;
      std::optional<Node> type_declaration;
      Node body;
    };
    struct Arrow {
      std::string_view position;
      Node domain;
      Node codomain;
    };
  };
};

#endif
