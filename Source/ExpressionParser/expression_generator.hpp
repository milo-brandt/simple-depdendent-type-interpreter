#ifndef EXPRESSION_GENERATOR_HPP
#define EXPRESSION_GENERATOR_HPP

#include "parser_tree.hpp"
#include "lexer_tree.hpp"

namespace expression_parser {
  using LexerIndex = lex_archive_index::Term;
  struct LexerSpan {
    LexerIndex parent;
  };
  struct ParseError {
    LexerIndex position;
    std::string message;
    std::vector<std::pair<LexerIndex, std::string> > secondary_positions;
  };
  namespace symbols {
    constexpr std::uint64_t block = 0;
    constexpr std::uint64_t declare = 1;
    constexpr std::uint64_t axiom = 2;
    constexpr std::uint64_t rule = 3;

    constexpr std::uint64_t arrow = 4;
    constexpr std::uint64_t colon = 5;
    constexpr std::uint64_t semicolon = 6;
    constexpr std::uint64_t equals = 7;
    constexpr std::uint64_t backslash = 8;
    constexpr std::uint64_t double_backslash = 9;
    constexpr std::uint64_t dot = 10;
    constexpr std::uint64_t underscore = 11;
  };
  mdb::Result<located_output::Expression, ParseError> parse_lexed(lex_output::archive_part::Term const&);
}

#endif
