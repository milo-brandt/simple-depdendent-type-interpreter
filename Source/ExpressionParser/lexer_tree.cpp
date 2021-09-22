#include <variant>
#include <cctype>
#include "lexer_tree.hpp"

namespace expression_parser {
  namespace {
    namespace tokens {
      enum class Fixed {
        open_paren, close_paren, open_brace, close_brace, eof, quote
      };
      enum class AtPointer {
        digit, letter
      };
      struct Symbol{
        std::uint64_t length;
        std::uint64_t symbol_index;
      };
      using Any = std::variant<Fixed, Symbol, AtPointer>;
    }
    mdb::Result<tokens::Any, LexerError> get_next_token(std::string_view& str, SymbolMap const& map) {
      auto token = [&](tokens::Any token, std::size_t skip_count = 0) {
        str.remove_prefix(skip_count);
        return token;
      };
    SKIP_WHITESPACE:
      if(std::isspace(str[0])) {
        str.remove_prefix(1);
        goto SKIP_WHITESPACE;
      }
      if(str[0] == '#') { //comment
        auto next_line = str.find_first_of("\n\r");
        if(next_line != std::string::npos) {
          str.remove_prefix(next_line + 1);
          goto SKIP_WHITESPACE;
        } else {
          return token(tokens::Fixed::eof, str.size());
        }
      }
      if(str[0] == '(') return token(tokens::Fixed::open_paren, 1);
      if(str[0] == ')') return token(tokens::Fixed::close_paren, 1);
      if(str[0] == '{') return token(tokens::Fixed::open_brace, 1);
      if(str[0] == '}') return token(tokens::Fixed::close_brace, 1);
      if(str[0] == '\"') return token(tokens::Fixed::quote, 1);
      if(std::isdigit(str[0])) return token(tokens::AtPointer::digit);
      { //search for symbols
        auto candidates_end = map.upper_bound(str); //first index greater than the string
        auto candidates_begin = map.lower_bound(str.substr(0, 1)); //first index at least the string's first character.
        for(auto it = candidates_begin; it != candidates_end; ++it) {
          if(str.starts_with(it->first)) {
            return token(tokens::Symbol{.length = it->first.size(), .symbol_index = it->second}, it->first.size());
          }
        }
      }
      if(std::isalpha(str[0]) || str[0] == '_') return token(tokens::AtPointer::letter);
      if(str.empty()) return token(tokens::Fixed::eof);
      return LexerError{
        str,
        "No token recognized."
      };
    }
    std::string_view string_between(const char* beg, const char* end) {
      return std::string_view{beg, std::size_t(end - beg)};
    }
    mdb::Result<std::string, LexerError> lex_string_literal(std::string_view& str) { //after first quote is removed
      std::string ret = "";
      auto full = str;
      auto extra_full = string_between(full.data() - 1, &*full.end());
    READ_CHARACTER:
      if(str.empty()) return LexerError{extra_full, "Unterminated string."};
      if(str[0] == '"') {
        str.remove_prefix(1);
        return std::move(ret);
      } else if(str[0] == '\\') {
        auto escape_full = str;
        str.remove_prefix(1);
        if(str.empty()) return LexerError{extra_full, "Unterminated string."};
        if(str[0] == 'n') ret += '\n';
        else if(str[0] == 't') ret += '\t';
        else if(str[0] == 'r') ret += '\r';
        else if(str[0] == '\\') ret += '\\';
        else if(str[0] == '"') ret += '"';
        else if(str[0] == '\'') ret += '\'';
        else {
          return LexerError{escape_full, "Unrecognized escape sequence."};
        }
        str.remove_prefix(1);
        goto READ_CHARACTER;
      } else {
        ret += str[0];
        str.remove_prefix(1);
        goto READ_CHARACTER;
      }
    }
    std::uint64_t parse_integer_literal(std::string_view& str) { //precondition: str[0] exists and is a digit.
      std::uint64_t ret = 0;
      while(!str.empty() && '0' <= str[0] && str[0] <= '9') {
        std::uint64_t digit = str[0] - '0';
        ret *= 10;
        ret += digit;
        str.remove_prefix(1);
      }
      return ret;
    }
    std::string_view parse_identifier(std::string_view& str) { //precondition: str[0] exists and is alpha or '_'
      std::string_view total = str;
      while(!str.empty() && (std::isalpha(str[0]) || str[0] == '_')) {
        str.remove_prefix(1);
      }
      return string_between(total.data(), str.data());
    }
    mdb::Result<std::vector<lex_located_output::Term>, LexerError> lex_string_with_end(std::string_view& str, tokens::Fixed expected_end, LexerInfo const& info) {
      std::vector<lex_located_output::Term> terms;
      std::string_view full = str;
      std::string_view extra_full = (expected_end == tokens::Fixed::eof) ? full : string_between(full.data() - 1, &*full.end());
    PARSE_NEXT_TOKEN:
      auto next_token_result = get_next_token(str, info.symbol_map);
      if(next_token_result.holds_error()) return std::move(next_token_result.get_error());
      auto& next_token = next_token_result.get_value();
      auto token_str = str;
      if(auto* fixed = std::get_if<tokens::Fixed>(&next_token)) {
        switch(*fixed) {
        case tokens::Fixed::open_paren:
          {
            auto next = lex_string_with_end(str, tokens::Fixed::close_paren, info);
            if(next.holds_error()) return std::move(next.get_error());
            terms.push_back(lex_located_output::ParenthesizedExpression{
              std::move(next.get_value()),
              string_between(token_str.data() - 1, str.data())
            });
            goto PARSE_NEXT_TOKEN;
          }
        case tokens::Fixed::open_brace:
          {
            auto next = lex_string_with_end(str, tokens::Fixed::close_brace, info);
            if(next.holds_error()) return std::move(next.get_error());
            terms.push_back(lex_located_output::CurlyBraceExpression{
              std::move(next.get_value()),
              string_between(token_str.data() - 1, str.data())
            });
            goto PARSE_NEXT_TOKEN;
          }
        case tokens::Fixed::quote:
        {
          auto text_result = lex_string_literal(str);
          if(text_result.holds_error()) return std::move(text_result.get_error());
          terms.push_back(lex_located_output::StringLiteral{
            .text = std::move(text_result.get_value()),
            .position = string_between(token_str.data() - 1, str.data())
          });
          goto PARSE_NEXT_TOKEN;
        }
        default:
          if(expected_end != *fixed) {
            if(expected_end == tokens::Fixed::eof) {
              return LexerError{
                string_between(token_str.data() - 1, token_str.data()),
                "Unmatched end bracket."
              };
            } else if(*fixed == tokens::Fixed::eof) {
              return LexerError{
                string_between(full.data() - 1, &*full.end()),
                "Unterminated group."
              };
            } else {
              return LexerError{
                string_between(full.data() - 1, str.data()),
                "Mismatched group."
              };
            }
          } else {
            return std::move(terms);
          }
        }
      } else if(auto* at_pointer = std::get_if<tokens::AtPointer>(&next_token)) {
        switch(*at_pointer) {
          case tokens::AtPointer::digit: {
            auto value = parse_integer_literal(str);
            terms.push_back(lex_located_output::IntegerLiteral{
              .value = value,
              .position = string_between(token_str.data(), str.data())
            });
            goto PARSE_NEXT_TOKEN;
          }
          case tokens::AtPointer::letter: {
            auto text = parse_identifier(str);
            terms.push_back(lex_located_output::Word{
              .text = text,
              .position = string_between(token_str.data(), str.data())
            });
            goto PARSE_NEXT_TOKEN;
          }
          default: std::terminate(); //unreachable
        }
      } else {
        auto const& sym = std::get<tokens::Symbol>(next_token);
        terms.push_back(lex_located_output::Symbol{
          .symbol_index = sym.symbol_index,
          .position = string_between(token_str.data() - sym.length, token_str.data())
        });
        goto PARSE_NEXT_TOKEN;
      }
      std::terminate(); //unreachable, I hope
    }
  }
  mdb::Result<lex_located_output::Term, LexerError> lex_string(std::string_view str, LexerInfo const& info) {
    auto full = str;
    return map(lex_string_with_end(str, tokens::Fixed::eof, info), [&](auto&& vec) -> lex_located_output::Term {
      return lex_located_output::ParenthesizedExpression{
        .body = std::move(vec),
        .position = string_between(full.data(), str.data())
      };
    });
  }
}
