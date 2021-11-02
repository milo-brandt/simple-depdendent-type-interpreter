#ifndef EXPRESSION_GENERATOR_HPP
#define EXPRESSION_GENERATOR_HPP

#include "parser_tree.hpp"
#include "lexer_tree.hpp"
#include "../Utility/function.hpp"

/*
match (f x) -> T {
  zero -> ...;
  succ n -> ...;
}
*/

namespace expression_parser {
  enum class ExpressionSymbol {
    block, declare, axiom, rule, let, where, match, verify, require, arrow, colon, semicolon, equals, backslash, double_backslash, dot, underscore, comma, unknown
  };
  using ExpressionSymbolMap = mdb::function<ExpressionSymbol(std::uint64_t)>;
  mdb::Result<located_output::Expression, ParseError> parse_lexed(lex_output::archive_root::Term const&, LexerSpanIndex, ExpressionSymbolMap);
  mdb::Result<located_output::Expression, ParseError> parse_lexed(lex_output::archive_root::Term const&, ExpressionSymbolMap);
}

#endif
