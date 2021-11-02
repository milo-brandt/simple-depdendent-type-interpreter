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
  struct LexerSpanIndex {
    lex_archive_index::Term parent;
    std::uint64_t begin_index;
    std::uint64_t end_index;
    friend bool operator==(LexerSpanIndex const& lhs, LexerSpanIndex const& rhs) {
      return lhs.parent == rhs.parent && lhs.begin_index == rhs.begin_index && lhs.end_index == rhs.end_index;
    }
  };
  class LexerLocatorSpan {
    using Iterator = lex_locator::archive_part::SpanTerm::ConstIterator;
    Iterator span_begin;
    Iterator span_end;
  public:
    LexerLocatorSpan(LexerSpanIndex, lex_locator::archive_root::Term const&);
    Iterator begin() const { return span_begin; }
    Iterator end() const { return span_end; }
    bool empty() const { return span_begin == span_end; }
  };
  std::string_view position_of(lex_locator::archive_part::Term const&);
  std::string_view position_of(LexerLocatorSpan const&);
  std::string_view position_of(LexerSpanIndex const&, lex_locator::archive_root::Term const&);

  mdb::Result<lex_located_output::Term, LexerError> lex_string(std::string_view, LexerInfo const&); //not really parenthesized; just a convenient way to return vector

  using LexerIndex = lex_archive_index::Term;
  struct ParseError {
    LexerIndex position;
    std::string message;
    std::vector<std::pair<LexerIndex, std::string> > secondary_positions;
  };
}

#endif
