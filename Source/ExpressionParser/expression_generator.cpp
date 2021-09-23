#include "expression_generator.hpp"
#include <algorithm>

namespace expression_parser {
  namespace {
    namespace lex_archive = lex_output::archive_part;
    using LexerIt = lex_archive::SpanTerm::ConstIterator;
    struct LexerSpan {
      LexerIt span_begin;
      LexerIt span_end;
      auto begin() { return span_begin; }
      auto end() { return span_end; }
      bool empty() { return !(span_begin != span_end); }
      LexerSpan(LexerIt span_begin, LexerIt span_end):span_begin(span_begin),span_end(span_end){}
      LexerSpan(lex_output::archive_part::ParenthesizedExpression const& expr):span_begin(expr.body.begin()),span_end(expr.body.end()) {}
      LexerSpan(lex_output::archive_part::CurlyBraceExpression const& expr):span_begin(expr.body.begin()),span_end(expr.body.end()) {}
    };
    using ExprResult = mdb::Result<output::Expression, ParseError>;
    using CommandResult = mdb::Result<output::Command, ParseError>;
    using PatternResult = mdb::Result<output::Pattern, ParseError>;

    ExprResult parse_expression(LexerSpan& span);
    ExprResult parse_block(lex_archive::CurlyBraceExpression const& span);
    ExprResult parse_lambda(LexerSpan& span) { //precondition: span is after backslash
      //Is: an optional variable name, followed, optionally, by : <type>, then by . <body>
      if(span.empty()) return ParseError{
        .position = (span.span_begin - 1)->index(),
        .message = "Lambda declaration before EOF."
      };
      std::optional<std::string_view> var_name;
      if(auto* name = span.span_begin->get_if_word()) {
        var_name = name->text;
        ++span.span_begin;
        if(span.empty()) return ParseError {
          .position = (span.span_begin - 1)->index(),
          .message = "Lambda declaration before EOF."
        };
      }
      std::optional<LexerSpan> type_span;
      if(span.span_begin->holds_symbol() && span.span_begin->get_symbol().symbol_index == symbols::colon) {
        auto type_begin = ++span.span_begin;
        std::uint64_t dots_needed = 1; //scan until we find the first . not cancelled by an earlier lambda.
        for(;!span.empty();++span.span_begin) {
          if(auto* symbol = span.span_begin->get_if_symbol()) {
            if(symbol->symbol_index == symbols::backslash) {
              ++dots_needed; //allows expressions such as "\x:F \y.y.x" - don't write code like this, though.
            } else if(symbol->symbol_index == symbols::dot) {
              if(--dots_needed == 0) {
                goto FOUND_LAMBDA_TYPE_END;
              }
            }
          }
        }
        return ParseError {
          .position = (type_begin - 1)->index(),
          .message = "Lambda type definition is not ended by a dot."
        };
      FOUND_LAMBDA_TYPE_END:
        type_span = LexerSpan{type_begin, span.span_begin};
        if(type_span->empty()) {
          return ParseError{
            .position = (type_span->span_begin - 1)->index(),
            .message = "No type given between ':' and '.'"
          };
        }
        //span_begin is now pointing at the period that ends the lambda (for sure)
      }
      if(span.span_begin->holds_symbol() && span.span_begin->get_symbol().symbol_index == symbols::dot) {
        ++span.span_begin;
        if(span.empty()) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Lambda has no body."
          };
        }
        if(type_span) {
          return mdb::gather_map([&](output::Expression type, output::Expression body) -> output::Expression {
            return output::Lambda{
              .body = std::move(body),
              .type = std::move(type),
              .arg_name = var_name
            };
          }, [&] { return parse_expression(*type_span); }, [&] { return parse_expression(span); });
        } else {
          return mdb::map(parse_expression(span), [&](output::Expression body) -> output::Expression {
            return output::Lambda {
              .body = std::move(body),
              .arg_name = var_name
            };
          });
        }
      } else {
        if(!var_name) {
          return ParseError {
            .position = span.span_begin->index(),
            .message = "Expected either a variable name, ':', or '.' after lambda declaration."
          };
        } else {
          return ParseError {
            .position = span.span_begin->index(),
            .message = "Expected either ':' or '.' after lambda argument name."
          };
        }
      }
    }
    bool should_parse_dependent_arrow(lex_archive::ParenthesizedExpression const& parens) {
      if(parens.body.size() >= 2) {
        return parens.body[0].holds_word()
            && parens.body[1].holds_symbol()
            && parens.body[1].get_symbol().symbol_index == symbols::colon;
      } else {
        return false;
      }
    }
    ExprResult parse_dependent_arrow(lex_archive::ParenthesizedExpression const& parens, LexerSpan& span) {
      LexerSpan parens_span{parens};
      auto arg_name = parens.body[0].get_word().text;
      parens_span.span_begin = parens_span.span_begin + 2;
      if(span.empty() || !span.span_begin->holds_symbol() || span.span_begin->get_symbol().symbol_index != symbols::arrow) {
        return ParseError {
          .position = (span.span_begin - 1)->index(),
          .message = "Expected an arrow after the dependent type expression."
        };
      }
      ++span.span_begin;
      if(span.empty()) {
        return ParseError {
          .position = (span.span_begin - 1)->index(),
          .message = "Expected an expression after '->'."
        };
      }
      if(parens_span.empty()) {
        return ParseError {
          .position = (parens_span.span_begin - 1)->index(),
          .message = "Expected an expression after ':'."
        };
      }
      return mdb::gather_map([&](output::Expression domain, output::Expression codomain) -> output::Expression {
        return output::Arrow{
          .domain = std::move(domain),
          .codomain = std::move(codomain),
          .arg_name = arg_name
        };
      }, [&] { return parse_expression(parens_span); }, [&] { return parse_expression(span); });
    }
    ExprResult parse_term(LexerSpan& span) { //precondition: span is not empty.
      auto const& head = *span.span_begin;
      ++span.span_begin;
      return head.visit(mdb::overloaded{
        [&](lex_archive::Symbol const& sym) -> ExprResult {
          switch(sym.symbol_index) {
          case symbols::block:
            if(span.empty()) {
              return ParseError {
                .position = head.index(),
                .message = "'block' at end of expression."
              };
            } else if(auto* brace = span.span_begin->get_if_curly_brace_expression()) {
              ++span.span_begin;
              return parse_block(*brace);
            } else {
              return ParseError {
                .position = head.index(),
                .message = "'block' not followed by braces."
              };
            }
          case symbols::double_backslash:
            if(span.empty()) {
              return ParseError {
                .position = head.index(),
                .message = "Expected expression after '\\\\'."
              };
            } else {
              return parse_expression(span); //right associative term
            }
          case symbols::backslash:
            return parse_lambda(span);
          default:
            return ParseError{
              .position = head.index(),
              .message = "Unexpected symbol in expression."
            };
          }
        },
        [&](lex_archive::Word const& word) -> ExprResult {
          return output::Expression{output::Identifier{
            .id = word.text
          }};
        },
        [&](lex_archive::StringLiteral const& literal) -> ExprResult {
          return output::Expression{output::Literal{
            .value = literal.text
          }};
        },
        [&](lex_archive::IntegerLiteral const& literal) -> ExprResult {
          return output::Expression{output::Literal{
            .value = literal.value
          }};
        },
        [&](lex_archive::ParenthesizedExpression const& parens) -> ExprResult {
          if(should_parse_dependent_arrow(parens)) {
            return parse_dependent_arrow(parens, span);
          } else {
            LexerSpan parens_span = parens;
            if(parens_span.empty()) {
              return ParseError {
                .position = head.index(),
                .message = "Expected expression inside parentheses."
              };
            } else {
              return parse_expression(parens_span);
            }
          }
        },
        [&](lex_archive::CurlyBraceExpression const& braces) -> ExprResult {
          return ParseError{
            .position = head.index(),
            .message = "Curly braces in expression."
          };
        }
      });
    }
    ExprResult parse_applications(LexerSpan& span) {
      //precondition: span is not empty.
      //will always either empty the span or leave it at an arrow.
      auto head_result = parse_term(span);
      if(head_result.holds_error()) return std::move(head_result);
      auto result = std::move(head_result.get_value());
      while(!span.empty()) {
        auto const& next_token = *span.span_begin;
        if(auto* symbol = next_token.get_if_symbol()) {
          if(symbol->symbol_index == symbols::arrow) {
            break; //don't parse through arrows (lower precedence)
          }
        }
        auto next_term = parse_term(span);
        if(next_term.holds_error()) return std::move(next_term);
        result = output::Apply{
          .lhs = std::move(result),
          .rhs = std::move(next_term.get_value())
        };
      }
      return result;
    }
    ExprResult parse_expression(LexerSpan& span) {
      //precondition: span is not empty. Must consume whole span.
      auto head_result = parse_applications(span);
      if(head_result.holds_error()) return std::move(head_result);
      if(span.empty()) {
        return std::move(head_result);
      } else {
        //span_begin is at an arrow.
        ++span.span_begin;
        if(span.empty()) {
          return ParseError {
            .position = (span.span_begin - 1)->index(),
            .message = "Expected expression after '->'."
          };
        } else {
          auto codomain = parse_expression(span);
          if(codomain.holds_error()) return std::move(codomain);
          return output::Expression{output::Arrow{
            .domain = std::move(head_result.get_value()),
            .codomain = std::move(codomain.get_value())
          }};
        }
      }
    }
    CommandResult parse_declare_or_axiom(LexerSpan& span, bool axiom) { //pointed *at* "declare"
      ++span.span_begin;
      if(span.empty() || !span.span_begin->holds_word()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected variable name after 'declare' or 'axiom'."
        };
      }
      std::string_view var_name = span.span_begin->get_word().text;
      ++span.span_begin;
      if(span.empty() || !span.span_begin->holds_symbol() || span.span_begin->get_symbol().symbol_index != symbols::colon) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected ':' after variable name in declaration."
        };
      }
      ++span.span_begin;
      if(span.empty()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected type expression after ':'."
        };
      }
      auto expr = parse_expression(span);
      if(expr.holds_error()) return std::move(expr.get_error());
      if(axiom) {
        return output::Command{output::Axiom{
          .type = std::move(expr.get_value()),
          .name = var_name
        }};
      } else {
        return output::Command{output::Declare{
          .type = std::move(expr.get_value()),
          .name = var_name
        }};
      }
    }
    template<class It, class Pred>
    It find_if(It begin, It end, Pred pred) {
      for(;begin != end;++begin) {
        if(pred(*begin)) return begin;
      }
      return end;
    }
    PatternResult parse_pattern(LexerSpan& span) {
      auto parse_term = [&]() -> PatternResult {
        if(auto* word = span.span_begin->get_if_word()) {
          ++span.span_begin;
          return output::Pattern{output::PatternIdentifier{
            .id = word->text
          }};
        } else if(span.span_begin->holds_symbol() && span.span_begin->get_symbol().symbol_index == symbols::underscore) {
          ++span.span_begin;
          return output::Pattern{output::PatternHole{}};
        } else if(auto* parens = span.span_begin->get_if_parenthesized_expression()) {
          ++span.span_begin;
          LexerSpan inner_span = *parens;
          return parse_pattern(inner_span);
        } else {
          return ParseError{
            .position = span.span_begin->index(),
            .message = "Expected identifier, '_', or '(' in pattern."
          };
        }
      };
      auto head = parse_term();
      if(head.holds_error()) return std::move(head);
      auto ret = std::move(head.get_value());
      while(!span.empty()) {
        auto next = parse_term();
        if(next.holds_error()) return std::move(next);
        ret = output::PatternApply{
          .lhs = std::move(ret),
          .rhs = std::move(next.get_value())
        };
      }
      return ret;
    }
    CommandResult parse_rule(LexerSpan& span) { //pointed *after* "rule"
      auto equals_sign = find_if(span.begin(), span.end(), [](lex_archive::Term const& term) {
        return term.holds_symbol() && term.get_symbol().symbol_index == symbols::equals;
      });
      if(equals_sign == span.end()) {
        return ParseError{
          .position = span.span_begin->index(),
          .message = "Rule declarations must include '='."
        };
      }
      LexerSpan pattern_span{span.span_begin, equals_sign};
      if(pattern_span.empty()) {
        return ParseError{
          .position = equals_sign->index(),
          .message = "Expected pattern before '='."
        };
      }
      auto pattern_result = parse_pattern(pattern_span);
      if(pattern_result.holds_error()) return std::move(pattern_result.get_error());
      LexerSpan replacement_span{equals_sign + 1, span.end()};
      if(replacement_span.empty()) {
        return ParseError{
          .position = equals_sign->index(),
          .message = "Expected expression after '='."
        };
      }
      auto replacement = parse_expression(replacement_span);
      if(replacement.holds_error()) return std::move(replacement.get_error());
      return output::Command{output::Rule{
        .pattern = std::move(pattern_result.get_value()),
        .replacement = std::move(replacement.get_value())
      }};
    }

    CommandResult parse_statement(LexerSpan& span, bool final) { //must consume span
      if(auto* symbol = span.span_begin->get_if_symbol()) {
        switch(symbol->symbol_index) {
          case symbols::declare: return parse_declare_or_axiom(span, false);
          case symbols::axiom: return parse_declare_or_axiom(span, true);
          case symbols::rule: {
            ++span.span_begin;
            if(span.empty()) {
              return ParseError{
                .position = (span.span_begin - 1)->index(),
                .message = "Expected equality after 'rule'."
              };
            } else {
              return parse_rule(span);
            }
          }
          default:
            ; //do nothing
        }
      }
      if(find_if(span.begin(), span.end(), [](lex_archive::Term const& term) {
        return term.holds_symbol() && term.get_symbol().symbol_index == symbols::equals;
      }) != span.end()) {
        return parse_rule(span);
      }
      if(final) {
        return ParseError{
          .position = span.span_begin->index(),
          .message = "Expected statement. If this is the block return, the semicolon at the end of the block must be removed."
        };
      } else {
        return ParseError{
          .position = span.span_begin->index(),
          .message = "Expected statement. Did not find a command nor an equals sign."
        };
      }
    }
    ExprResult parse_block(lex_archive::CurlyBraceExpression const& braces) {
      std::vector<LexerIt> expression_splits;
      LexerSpan span = braces;
      if(span.empty()) {
        return ParseError{
          .position = braces.index(),
          .message = "Expected statements and expression in block."
        };
      }
      std::vector<output::Command> commands;
      while(true) {
        auto next_semi = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && term.get_symbol().symbol_index == symbols::semicolon;
        });
        if(next_semi == span.end()) break;
        LexerSpan command_span{span.begin(), next_semi};
        if(command_span.empty()) continue; //ignore empty statements
        auto next_command_result = parse_statement(command_span, next_semi + 1 == span.end());
        if(next_command_result.holds_error()) return std::move(next_command_result.get_error());
        commands.push_back(std::move(next_command_result.get_value()));
        span.span_begin = next_semi + 1;
      }
      if(span.empty()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected expression after last ';' in block."
        };
      }
      auto expr_result = parse_expression(span);
      if(expr_result.holds_error()) return std::move(expr_result);
      return output::Expression{output::Block{
        .statements = std::move(commands),
        .value = std::move(expr_result.get_value())
      }};
    }
    mdb::Result<output::Expression, ParseError> parse_lexed_impl(lex_output::archive_part::Term const& term) {
      LexerSpan span = term.get_parenthesized_expression();
      return parse_expression(span);
    }
  }
  mdb::Result<output::Expression, ParseError> parse_lexed(lex_output::archive_part::Term const& term) {
    return parse_lexed_impl(term);
  }
}
