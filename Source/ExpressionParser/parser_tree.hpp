#ifndef PARSER_TREE_HPP
#define PARSER_TREE_HPP

#include "parser_tree_impl.hpp"
#include "resolution_context_impl.hpp"
#include "../Utility/result.hpp"

namespace expression_parser::locator {
  template<class T> std::string_view position_of(T const& expr) {
    return expr.visit([](auto const& x) { return x.position; });
  }
}
namespace expression_parser {
  struct ResolutionError {
    std::vector<archive_index::Identifier> bad_ids;
    std::vector<archive_index::PatternIdentifier> bad_pattern_ids;
  };
  mdb::Result<resolved::Expression, ResolutionError> resolve(resolved::Context context, output::archive_part::Expression const&);
  mdb::Result<resolved::Command, ResolutionError> resolve(resolved::Context context, output::archive_part::Command const&);
}

#endif
