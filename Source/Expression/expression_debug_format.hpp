#ifndef EXPRESSION_DEBUG_FORMAT_HPP
#define EXPRESSION_DEBUG_FORMAT_HPP

#include "expression_tree.hpp"
#include "../Utility/function.hpp"

namespace expression {
  struct RawFormat {
    tree::Expression const& expression;
    mdb::function<void(std::ostream&, std::uint64_t)> format_external;
  };
  RawFormat raw_format(tree::Expression const&);
  RawFormat raw_format(tree::Expression const&, mdb::function<void(std::ostream&, std::uint64_t)> format_external);
  std::ostream& operator<<(std::ostream&, RawFormat const&);
}

#endif
