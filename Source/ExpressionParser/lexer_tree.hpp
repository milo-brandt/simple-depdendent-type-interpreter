#ifndef LEXER_TREE_HPP
#define LEXER_TREE_HPP

#include "lexer_tree_impl.hpp"
#include "../Utility/result.hpp"
#include <map>

namespace expression_parser {
  struct LexerError {
    std::string_view position;
    std::string message;
  };
  struct StringCompare {
    using is_transparent = void;
    bool operator()(std::string const& lhs, std::string const& rhs) const { return lhs < rhs; }
    bool operator()(std::string const& lhs, std::string_view const& rhs) const { return lhs < rhs; }
    bool operator()(std::string_view const& lhs, std::string const& rhs) const { return lhs < rhs; }
  };
  using SymbolMap = std::map<std::string, std::uint64_t, StringCompare>;
  struct LexerInfo {
    SymbolMap symbol_map;
  };
  mdb::Result<lex_located_output::Term, LexerError> lex_string(std::string_view, LexerInfo const&); //not really parenthesized; just a convenient way to return vector
}

#endif
