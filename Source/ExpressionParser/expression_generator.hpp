#ifndef EXPRESSION_GENERATOR_HPP
#define EXPRESSION_GENERATOR_HPP

#include "parser_tree.hpp"
#include "lexer_tree.hpp"

/*
match (f x) -> T {
  zero -> ...;
  succ n -> ...;
}
*/

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
    constexpr std::uint64_t let = 4;
    constexpr std::uint64_t where = 14;
    constexpr std::uint64_t match = 15;
    constexpr std::uint64_t verify = 16;
    constexpr std::uint64_t require = 17;

    constexpr std::uint64_t arrow = 5;
    constexpr std::uint64_t colon = 6;
    constexpr std::uint64_t semicolon = 7;
    constexpr std::uint64_t equals = 8;
    constexpr std::uint64_t backslash = 9;
    constexpr std::uint64_t double_backslash = 10;
    constexpr std::uint64_t dot = 11;
    constexpr std::uint64_t underscore = 12;
    constexpr std::uint64_t comma = 13;
  };
  mdb::Result<located_output::Expression, ParseError> parse_lexed(lex_output::archive_part::Term const&);
}

#endif
