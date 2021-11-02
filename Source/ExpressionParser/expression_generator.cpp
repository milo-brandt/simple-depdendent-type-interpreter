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
      LexerSpan(lex_archive::ParenthesizedExpression const& expr):span_begin(expr.body.begin()),span_end(expr.body.end()) {}
      LexerSpan(lex_archive::BraceExpression const& expr):span_begin(expr.body.begin()),span_end(expr.body.end()) {}
      LexerSpan(lex_archive::BracketExpression const& expr):span_begin(expr.body.begin()),span_end(expr.body.end()) {}
    };
    struct LocatorInfo {
      lex_archive_index::Term parent;
      LexerIt parent_begin;
      LocatorInfo(lex_archive::ParenthesizedExpression const& expr):parent(expr.index()),parent_begin(expr.body.begin()) {}
      LocatorInfo(lex_archive::BraceExpression const& expr):parent(expr.index()),parent_begin(expr.body.begin()) {}
      LocatorInfo(lex_archive::BracketExpression const& expr):parent(expr.index()),parent_begin(expr.body.begin()) {}
      LexerSpanIndex to_span(LexerSpan input) const {
        return {
          .parent = parent,
          .begin_index = (std::uint64_t)(input.span_begin - parent_begin),
          .end_index = (std::uint64_t)(input.span_end - parent_begin)
        };
      }
      LexerSpanIndex to_span(LexerIt begin, LexerIt end) const {
        return {
          .parent = parent,
          .begin_index = (std::uint64_t)(begin - parent_begin),
          .end_index = (std::uint64_t)(end - parent_begin)
        };
      }
    };
    using ExprResult = mdb::Result<located_output::Expression, ParseError>;
    using CommandResult = mdb::Result<located_output::Command, ParseError>;
    using PatternResult = mdb::Result<located_output::Pattern, ParseError>;

    template<class It, class Pred>
    It find_if(It begin, It end, Pred pred) {
      for(;begin != end;++begin) {
        if(pred(*begin)) return begin;
      }
      return end;
    }
    ExprResult parse_expression(LocatorInfo const&, LexerSpan& span, ExpressionSymbolMap& symbol_map);
    ExprResult parse_block(LexerSpanIndex position, lex_archive::BraceExpression const& braces, ExpressionSymbolMap& symbol_map);
    ExprResult parse_match(LocatorInfo const&, LexerSpan& span, ExpressionSymbolMap& symbol_map);
    ExprResult parse_vector_literal(LexerSpanIndex position, lex_archive::BracketExpression const& bracket, ExpressionSymbolMap& symbol_map) {
      LexerSpan span = bracket;
      LocatorInfo locator = bracket;
      std::vector<located_output::Expression> elements;
      while(true) {
        auto next_comma = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::comma;
        });
        LexerSpan element_span{span.begin(), next_comma};
        if(element_span.empty()) {
          if(next_comma == span.end()) break;
          else {
            ++span.begin();
            continue;
          }
        }
        auto next_element_result = parse_expression(locator, element_span, symbol_map);
        if(next_element_result.holds_error()) return std::move(next_element_result.get_error());
        elements.push_back(std::move(next_element_result.get_value()));
        if(next_comma == span.end()) break;
        span.span_begin = next_comma + 1;
      }
      return located_output::Expression{located_output::VectorLiteral{
        .elements = std::move(elements),
        .position = position
      }};
    }
    ExprResult parse_lambda(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) { //precondition: span is after backslash
      //Is: an optional variable name, followed, optionally, by : <type>, then by . <body>
      auto lambda_start = span.span_begin - 1;
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
      if(span.span_begin->holds_symbol() && symbol_map(span.span_begin->get_symbol().symbol_index) == ExpressionSymbol::colon) {
        auto type_begin = ++span.span_begin;
        std::uint64_t dots_needed = 1; //scan until we find the first . not cancelled by an earlier lambda.
        for(;!span.empty();++span.span_begin) {
          if(auto* symbol = span.span_begin->get_if_symbol()) {
            if(symbol_map(symbol->symbol_index) == ExpressionSymbol::backslash) {
              ++dots_needed; //allows expressions such as "\x:F \y.y.x" - don't write code like this, though.
            } else if(symbol_map(symbol->symbol_index) == ExpressionSymbol::dot) {
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
      if(span.span_begin->holds_symbol() && symbol_map(span.span_begin->get_symbol().symbol_index) == ExpressionSymbol::dot) {
        ++span.span_begin;
        if(span.empty()) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Lambda has no body."
          };
        }
        if(type_span) {
          return mdb::gather_map([&](located_output::Expression type, located_output::Expression body) -> located_output::Expression {
            return located_output::Lambda{
              .body = std::move(body),
              .type = std::move(type),
              .arg_name = var_name,
              .position = locator.to_span(lambda_start, span.span_begin)
            };
          }, [&] { return parse_expression(locator, *type_span, symbol_map); }, [&] { return parse_expression(locator, span, symbol_map); });
        } else {
          return mdb::map(parse_expression(locator, span, symbol_map), [&](located_output::Expression body) -> located_output::Expression {
            return located_output::Lambda {
              .body = std::move(body),
              .arg_name = var_name,
              .position = locator.to_span(lambda_start, span.span_begin)
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
    bool should_parse_dependent_arrow(lex_archive::ParenthesizedExpression const& parens, ExpressionSymbolMap& symbol_map) {
      if(parens.body.size() >= 2) {
        return parens.body[0].holds_word()
            && parens.body[1].holds_symbol()
            && symbol_map(parens.body[1].get_symbol().symbol_index) == ExpressionSymbol::colon;
      } else {
        return false;
      }
    }
    ExprResult parse_dependent_arrow(LocatorInfo const& locator, lex_archive::ParenthesizedExpression const& parens, LexerSpan& span, ExpressionSymbolMap& symbol_map) {
      auto dependent_arrow_begin = span.span_begin - 1;
      LexerSpan parens_span{parens};
      auto arg_name = parens.body[0].get_word().text;
      parens_span.span_begin = parens_span.span_begin + 2;
      if(span.empty() || !span.span_begin->holds_symbol() || symbol_map(span.span_begin->get_symbol().symbol_index) != ExpressionSymbol::arrow) {
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
      return mdb::gather_map([&](located_output::Expression domain, located_output::Expression codomain) -> located_output::Expression {
        return located_output::Arrow{
          .domain = std::move(domain),
          .codomain = std::move(codomain),
          .arg_name = arg_name,
          .position = locator.to_span(dependent_arrow_begin, span.span_begin)
        };
      }, [&] { return parse_expression(parens, parens_span, symbol_map); }, [&] { return parse_expression(locator, span, symbol_map); });
    }
    ExprResult parse_term(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) { //precondition: span is not empty.
      auto const& head = *span.span_begin;
      ++span.span_begin;
      return head.visit(mdb::overloaded{
        [&](lex_archive::Symbol const& sym) -> ExprResult {
          switch(symbol_map(sym.symbol_index)) {
          case ExpressionSymbol::block:
            if(span.empty()) {
              return ParseError {
                .position = head.index(),
                .message = "'block' at end of expression."
              };
            } else if(auto* brace = span.span_begin->get_if_brace_expression()) {
              ++span.span_begin;
              return parse_block(locator.to_span(span.span_begin - 2, span.span_begin), *brace, symbol_map);
            } else {
              return ParseError {
                .position = head.index(),
                .message = "'block' not followed by braces."
              };
            }
          case ExpressionSymbol::double_backslash:
            if(span.empty()) {
              return ParseError {
                .position = head.index(),
                .message = "Expected expression after '\\\\'."
              };
            } else {
              return parse_expression(locator, span, symbol_map); //right associative term
            }
          case ExpressionSymbol::backslash:
            return parse_lambda(locator, span, symbol_map);
          case ExpressionSymbol::underscore:
            return located_output::Expression{located_output::Hole{
              .position = locator.to_span(span.span_begin - 1, span.span_begin)
            }};
          case ExpressionSymbol::match:
            return parse_match(locator, span, symbol_map);
          default:
            return ParseError{
              .position = head.index(),
              .message = "Unexpected symbol in expression."
            };
          }
        },
        [&](lex_archive::Word const& word) -> ExprResult {
          return located_output::Expression{located_output::Identifier{
            .id = word.text,
            .position = locator.to_span(span.span_begin - 1, span.span_begin)
          }};
        },
        [&](lex_archive::StringLiteral const& literal) -> ExprResult {
          return located_output::Expression{located_output::Literal{
            .value = literal.text,
            .position = locator.to_span(span.span_begin - 1, span.span_begin)
          }};
        },
        [&](lex_archive::IntegerLiteral const& literal) -> ExprResult {
          return located_output::Expression{located_output::Literal{
            .value = literal.value,
            .position = locator.to_span(span.span_begin - 1, span.span_begin)
          }};
        },
        [&](lex_archive::ParenthesizedExpression const& parens) -> ExprResult {
          if(should_parse_dependent_arrow(parens, symbol_map)) {
            return parse_dependent_arrow(locator, parens, span, symbol_map);
          } else {
            LexerSpan parens_span = parens;
            if(parens_span.empty()) {
              return ParseError {
                .position = head.index(),
                .message = "Expected expression inside parentheses."
              };
            } else {
              return parse_expression(parens, parens_span, symbol_map);
            }
          }
        },
        [&](lex_archive::BraceExpression const& braces) -> ExprResult {
          return ParseError{
            .position = head.index(),
            .message = "Curly braces in expression."
          };
        },
        [&](lex_archive::BracketExpression const& bracket) -> ExprResult {
          return parse_vector_literal(locator.to_span(span.span_begin - 1, span.span_begin), bracket, symbol_map);
        }
      });
    }
    ExprResult parse_applications(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) {
      //precondition: span is not empty.
      //will always either empty the span or leave it at an arrow.
      auto apply_begin = span.begin();
      auto head_result = parse_term(locator, span, symbol_map);
      if(head_result.holds_error()) return std::move(head_result);
      auto result = std::move(head_result.get_value());
      while(!span.empty()) {
        auto const& next_token = *span.span_begin;
        if(auto* symbol = next_token.get_if_symbol()) {
          if(symbol_map(symbol->symbol_index) == ExpressionSymbol::arrow) {
            break; //don't parse through arrows (lower precedence)
          }
        }
        auto next_term = parse_term(locator, span, symbol_map);
        if(next_term.holds_error()) return std::move(next_term);
        result = located_output::Apply{
          .lhs = std::move(result),
          .rhs = std::move(next_term.get_value()),
          .position = locator.to_span(apply_begin, span.span_begin)
        };
      }
      return result;
    }
    ExprResult parse_expression(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) {
      //precondition: span is not empty. Must consume whole span.
      auto expr_begin = span.begin();
      auto head_result = parse_applications(locator, span, symbol_map);
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
          auto codomain = parse_expression(locator, span, symbol_map);
          if(codomain.holds_error()) return std::move(codomain);
          return located_output::Expression{located_output::Arrow{
            .domain = std::move(head_result.get_value()),
            .codomain = std::move(codomain.get_value()),
            .position = locator.to_span(expr_begin, span.span_begin)
          }};
        }
      }
    }
    CommandResult parse_declare_or_axiom(LocatorInfo const& locator, LexerSpan& span, bool axiom, ExpressionSymbolMap& symbol_map) { //pointed *at* "declare"
      auto declare_start = span.begin();
      ++span.span_begin;
      if(span.empty() || !span.span_begin->holds_word()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected variable name after 'declare' or 'axiom'."
        };
      }
      std::string_view var_name = span.span_begin->get_word().text;
      ++span.span_begin;
      if(span.empty() || !span.span_begin->holds_symbol() || symbol_map(span.span_begin->get_symbol().symbol_index) != ExpressionSymbol::colon) {
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
      auto expr = parse_expression(locator, span, symbol_map);
      if(expr.holds_error()) return std::move(expr.get_error());
      if(axiom) {
        return located_output::Command{located_output::Axiom{
          .type = std::move(expr.get_value()),
          .name = var_name,
          .position = locator.to_span(declare_start, span.span_begin)
        }};
      } else {
        return located_output::Command{located_output::Declare{
          .type = std::move(expr.get_value()),
          .name = var_name,
          .position = locator.to_span(declare_start, span.span_begin)
        }};
      }
    }
    CommandResult parse_let(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) { //pointed *at* "let"
      auto let_start = span.begin();
      ++span.span_begin;
      if(span.empty() || !span.span_begin->holds_word()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected variable name after 'declare' or 'axiom'."
        };
      }
      std::string_view var_name = span.span_begin->get_word().text;
      ++span.span_begin;
      std::optional<located_output::Expression> let_type;
      if(!span.empty() && span.span_begin->holds_symbol() && symbol_map(span.span_begin->get_symbol().symbol_index) == ExpressionSymbol::colon) {
        auto equals_sign = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::equals;
        });
        if(equals_sign == span.end()) {
          return ParseError{
            .position = span.span_begin->index(),
            .message = "Let statement does not contain '='."
          };
        }
        ++span.span_begin;
        if(span.span_begin == equals_sign) {
          return ParseError {
            .position = (span.span_begin - 1)->index(),
            .message = "Expected type between ':' and '='."
          };
        }
        LexerSpan type_span{span.span_begin, equals_sign};
        auto type = parse_expression(locator, type_span, symbol_map);
        if(type.holds_error()) return std::move(type.get_error());
        let_type = std::move(type.get_value());
        span.span_begin = equals_sign;
      } else if(span.empty() || !span.span_begin->holds_symbol() || symbol_map(span.span_begin->get_symbol().symbol_index) != ExpressionSymbol::equals) {
        return ParseError {
          .position = (span.span_begin - 1)->index(),
          .message = "Expected '=' or ':' after let variable name."
        };
      }
      //we are now pointed at an equals sign...
      ++span.span_begin;
      if(span.empty()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected expression after '='."
        };
      }
      auto expr = parse_expression(locator, span, symbol_map);
      if(expr.holds_error()) return std::move(expr.get_error());
      return located_output::Command{located_output::Let{
        .value = std::move(expr.get_value()),
        .type = std::move(let_type),
        .name = var_name,
        .position = locator.to_span(let_start, span.span_begin)
      }};
    }
    PatternResult parse_pattern(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) {
      auto parse_term = [&]() -> PatternResult {
        if(auto* word = span.span_begin->get_if_word()) {
          ++span.span_begin;
          return located_output::Pattern{located_output::PatternIdentifier{
            .id = word->text,
            .position = locator.to_span(span.span_begin - 1, span.span_begin)
          }};
        } else if(span.span_begin->holds_symbol() && symbol_map(span.span_begin->get_symbol().symbol_index) == ExpressionSymbol::underscore) {
          ++span.span_begin;
          return located_output::Pattern{located_output::PatternHole{
            .position = locator.to_span(span.span_begin - 1, span.span_begin)
          }};
        } else if(auto* parens = span.span_begin->get_if_parenthesized_expression()) {
          ++span.span_begin;
          LexerSpan inner_span = *parens;
          return parse_pattern(*parens, inner_span, symbol_map);
        } else {
          return ParseError{
            .position = span.span_begin->index(),
            .message = "Expected identifier, '_', or '(' in pattern."
          };
        }
      };
      auto pattern_begin = span.begin();
      auto head = parse_term();
      if(head.holds_error()) return std::move(head);
      auto ret = std::move(head.get_value());
      while(!span.empty()) {
        auto next = parse_term();
        if(next.holds_error()) return std::move(next);
        ret = located_output::PatternApply{
          .lhs = std::move(ret),
          .rhs = std::move(next.get_value()),
          .position = locator.to_span(pattern_begin, span.span_begin)
        };
      }
      return ret;
    }
    CommandResult parse_rule(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) { //pointed *after* "rule"
      auto primary_pattern_end = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
        return term.holds_symbol() && (
           symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::equals
        || symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::where
        );
      });
      if(primary_pattern_end == span.end()) {
        return ParseError{
          .position = span.span_begin->index(),
          .message = "Rule declaration does not contain '='."
        };
      }
      LexerSpan pattern_span{span.span_begin, primary_pattern_end};
      if(pattern_span.empty()) {
        if(symbol_map(primary_pattern_end->get_symbol().symbol_index) == ExpressionSymbol::equals) {
          return ParseError{
            .position = primary_pattern_end->index(),
            .message = "Expected pattern before '='."
          };
        } else {
          return ParseError{
            .position = primary_pattern_end->index(),
            .message = "Expected pattern before 'where'."
          };
        }
      }
      auto pattern_result = parse_pattern(locator, pattern_span, symbol_map);
      if(pattern_result.holds_error()) return std::move(pattern_result.get_error());
      auto subclause_end = primary_pattern_end;
      std::vector<located_output::Expression> subclause_expressions;
      std::vector<located_output::Pattern> subclause_patterns;
      while(symbol_map(subclause_end->get_symbol().symbol_index) == ExpressionSymbol::where) {
        auto subclause_begin = subclause_end + 1;
        auto expr_end = find_if(subclause_begin, span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && (
             symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::equals
          || symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::where
          );
        });
        if(expr_end == span.end()) {
          return ParseError{
            .position = subclause_end->index(),
            .message = "Expected '=' after 'where' in subclause."
          };
        }
        if(symbol_map(expr_end->get_symbol().symbol_index) == ExpressionSymbol::where) {
          return ParseError{
            .position = subclause_end->index(),
            .message = "Expected '=' before 'where' in subclause."
          };
        }
        subclause_end = find_if(expr_end + 1, span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && (
             symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::equals
          || symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::where
          );
        });
        if(subclause_end == span.end()) {
          return ParseError{
            .position = subclause_end->index(),
            .message = "Expected another 'where' clause or '=' after subclause."
          };
        }
        LexerSpan expr_span{subclause_begin, expr_end};
        auto subclause_expr = parse_expression(locator, expr_span, symbol_map);
        if(subclause_expr.holds_error()) return std::move(subclause_expr.get_error());
        LexerSpan pattern_span{expr_end + 1, subclause_end};
        auto subclause_pattern = parse_pattern(locator, pattern_span, symbol_map);
        subclause_expressions.push_back(std::move(subclause_expr.get_value()));
        subclause_patterns.push_back(std::move(subclause_pattern.get_value()));
      }
      LexerSpan replacement_span{subclause_end + 1, span.end()};
      if(replacement_span.empty()) {
        return ParseError{
          .position = subclause_end->index(),
          .message = "Expected expression after '='."
        };
      }
      auto replacement = parse_expression(locator, replacement_span, symbol_map);
      if(replacement.holds_error()) return std::move(replacement.get_error());
      return located_output::Command{located_output::Rule{
        .pattern = std::move(pattern_result.get_value()),
        .subclause_expressions = std::move(subclause_expressions),
        .subclause_patterns = std::move(subclause_patterns),
        .replacement = std::move(replacement.get_value()),
        .position = locator.to_span(span)
      }};
    }
    CommandResult parse_require_or_verify(LocatorInfo const& locator, LexerSpan& span, bool is_require, ExpressionSymbolMap& symbol_map) { //pointed *at* "require"/"verify"
      auto statement_begin = span.begin();
      ++span.span_begin;
      if(span.empty()) {
        return ParseError{
          .position = statement_begin->index(),
          .message = "Expected expression after 'require' or 'verify'."
        };
      }
      LexerIt expected_begin = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
        return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::equals;
      });
      LexerIt expected_type_begin = statement_begin;
      std::size_t lambda_def_depth = 0; //to ensure that "verify \x:Type.x : Type -> Type; is okay". Number of ways in which we are between a backslash and a period.
      for(;expected_type_begin != expected_begin; ++expected_type_begin) {
        if(auto* sym = expected_type_begin->get_if_symbol()) {
          if(symbol_map(sym->symbol_index) == ExpressionSymbol::backslash) {
            ++lambda_def_depth;
          } else if(symbol_map(sym->symbol_index) == ExpressionSymbol::dot) {
            if(lambda_def_depth > 0) {
              --lambda_def_depth;
            } else {
              return ParseError{
                .position = expected_type_begin->index(),
                .message = "Unexpected '.' in expression, not matched by opening lambda."
              };
            }
          } else if(symbol_map(sym->symbol_index) == ExpressionSymbol::colon && lambda_def_depth == 0) {
            break;
          }
        }
      }
      LexerSpan expr_span{span.begin(), expected_type_begin};
      auto expr = parse_expression(locator, expr_span, symbol_map);
      if(expr.holds_error()) return std::move(expr.get_error());
      std::optional<located_output::Expression> expected_type;
      std::optional<located_output::Expression> expected;
      if(expected_type_begin != expected_begin) {
        LexerSpan expected_type_span{expected_type_begin + 1, expected_begin};
        if(expected_type_span.empty()) {
          return ParseError{
            .position = expected_type_begin->index(),
            .message = "Expected expression for expected type after ':' in requirement."
          };
        }
        auto expected_type_result = parse_expression(locator, expected_type_span, symbol_map);
        if(expected_type_result.holds_error()) return std::move(expected_type_result.get_error());
        expected_type = std::move(expected_type_result.get_value());
      }
      if(expected_begin != span.end()) {
        LexerSpan expected_span{expected_begin + 1, span.end()};
        if(expected_span.empty()) {
          return ParseError{
            .position = expected_type_begin->index(),
            .message = "Expected expression for expected value after '=' in requirement."
          };
        }
        auto expected_result = parse_expression(locator, expected_span, symbol_map);
        if(expected_result.holds_error()) return std::move(expected_result.get_error());
        expected = std::move(expected_result.get_value());
      }
      return located_output::Command{located_output::Check{
        .term = std::move(expr.get_value()),
        .expected = std::move(expected),
        .expected_type = std::move(expected_type),
        .allow_deduction = is_require,
        .position = locator.to_span({statement_begin, span.end()})
      }};
    }
    CommandResult parse_statement(LocatorInfo const& locator, LexerSpan& span, bool final, ExpressionSymbolMap& symbol_map) { //must consume span
      if(auto* symbol = span.span_begin->get_if_symbol()) {
        switch(symbol_map(symbol->symbol_index)) {
          case ExpressionSymbol::declare: return parse_declare_or_axiom(locator, span, false, symbol_map);
          case ExpressionSymbol::axiom: return parse_declare_or_axiom(locator, span, true, symbol_map);
          case ExpressionSymbol::require: return parse_require_or_verify(locator, span, true, symbol_map);
          case ExpressionSymbol::verify: return parse_require_or_verify(locator, span, false, symbol_map);
          case ExpressionSymbol::let: return parse_let(locator, span, symbol_map);
          case ExpressionSymbol::rule: {
            ++span.span_begin;
            if(span.empty()) {
              return ParseError{
                .position = (span.span_begin - 1)->index(),
                .message = "Expected equality after 'rule'."
              };
            } else {
              return parse_rule(locator, span, symbol_map);
            }
          }
          default:
            ; //do nothing
        }
      }
      if(find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
        return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::equals;
      }) != span.end()) {
        return parse_rule(locator, span, symbol_map);
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
    struct InnerMatchInfo {
      std::vector<located_output::Pattern> arm_patterns;
      std::vector<located_output::Expression> arm_expressions;
    };
    mdb::Result<std::pair<located_output::Pattern, located_output::Expression>, ParseError> parse_match_arm(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) {
      //span is assumed non-empty
      auto mid_arrow = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
        return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::arrow;
      });
      if(mid_arrow == span.end()) {
        return ParseError{
          .position = span.span_begin->index(),
          .message = "Expected '->' in match arm."
        };
      }
      if(mid_arrow == span.span_begin) {
        return ParseError{
          .position = span.span_begin->index(),
          .message = "Expected pattern before '->' in match arm."
        };
      }
      LexerSpan pattern_span{span.span_begin, mid_arrow};
      auto pat = parse_pattern(locator, pattern_span, symbol_map);
      if(pat.holds_error()) return std::move(pat.get_error());
      LexerSpan expr_span{mid_arrow + 1, span.span_end};
      if(expr_span.empty()) {
        return ParseError {
          .position = mid_arrow->index(),
          .message = "Expected expression after '->' in match arm."
        };
      }
      auto expr = parse_expression(locator, expr_span, symbol_map);
      if(expr.holds_error()) return std::move(expr.get_error());
      return std::make_pair(
        std::move(pat.get_value()),
        std::move(expr.get_value())
      );
    }
    mdb::Result<InnerMatchInfo, ParseError> parse_match_body(lex_archive::BraceExpression const& braces, ExpressionSymbolMap& symbol_map) {
      LexerSpan span = braces;
      LocatorInfo locator = braces;
      /*if(span.empty()) {
        return ParseError{
          .position = braces.index(),
          .message = "Expected statements and expression in block."
        };
      }*/ //Technically, there are types that can be matched with no arms.
      InnerMatchInfo ret;
      while(true) {
        auto next_semi = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::semicolon;
        });
        if(next_semi == span.end()) {
          if(span.empty()) {
            return std::move(ret);
          } else {
            return ParseError {
              .position = span.span_begin->index(),
              .message = "Unexpected token between last semicolon of match statement and closing brace."
            };
          }
        }
        LexerSpan arm_span{span.span_begin, next_semi};
        if(arm_span.empty()) {
          return ParseError {
            .position = next_semi->index(),
            .message = "Expected match arm before ';'."
          };
        }
        auto arm = parse_match_arm(locator, arm_span, symbol_map);
        if(arm.holds_error()) return std::move(arm.get_error());
        auto& [arm_pattern, arm_expression] = arm.get_value();
        ret.arm_patterns.push_back(std::move(arm_pattern));
        ret.arm_expressions.push_back(std::move(arm_expression));
        span.span_begin = next_semi + 1;
      }
    }

    ExprResult parse_match(LocatorInfo const& locator, LexerSpan& span, ExpressionSymbolMap& symbol_map) { //span starts after "match" symbol
      auto match_begin = span.begin() - 1;
      if(span.empty() || !span.span_begin->holds_parenthesized_expression()) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected parenthesized expression after 'match'."
        };
      }
      auto& parenthesized_expr = span.span_begin->get_parenthesized_expression();
      LexerSpan parens_span = parenthesized_expr;
      auto matched_expr_result = parse_expression(parenthesized_expr, parens_span, symbol_map);
      if(matched_expr_result.holds_error()) return std::move(matched_expr_result.get_error());
      auto& matched_expr = matched_expr_result.get_value();
      ++span.span_begin;
      bool holds_arrow = !span.empty() && span.span_begin->holds_symbol() && symbol_map(span.span_begin->get_symbol().symbol_index) == ExpressionSymbol::arrow;
      bool holds_brace = !span.empty() && span.span_begin->holds_brace_expression();
      if(span.empty() || !(holds_arrow || holds_brace)) {
        return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected '->' or '{' after matched expression."
        };
      }

      std::optional<located_output::Expression> type_of_match;
      if(holds_arrow) {
        ++span.span_begin;
        bool was_block = false;
        auto match_brace = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
          if(was_block) {
            was_block = false;
            return false;
          } else {
            was_block = term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::block;
            return term.holds_brace_expression();
          }
        });
        if(match_brace == span.begin()) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Expected type after '->' in match statement."
          };
        }
        if(match_brace == span.end()) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Expected '{' after type declaration of match."
          };
        }
        LexerSpan type_span{span.span_begin, match_brace};
        auto type_result = parse_expression(locator, type_span, symbol_map);
        if(type_result.holds_error()) return std::move(type_result.get_error());
        type_of_match = std::move(type_result.get_value());
        span.span_begin = match_brace;
      }
      //span.begin() must point at a brace expression now.
      auto const& brace = span.span_begin->get_brace_expression();
      ++span.span_begin; //consume brace.
      auto body = parse_match_body(brace, symbol_map);
      if(body.holds_error()) return std::move(body.get_error());
      auto& [arm_patterns, arm_expressions] = body.get_value();

      return located_output::Expression{located_output::Match{
        .matched_expression = std::move(matched_expr),
        .output_type = std::move(type_of_match),
        .arm_patterns = std::move(arm_patterns),
        .arm_expressions = std::move(arm_expressions),
        .position = locator.to_span(match_begin, span.span_begin)
      }};
    }
    ExprResult parse_block(LexerSpanIndex position, lex_archive::BraceExpression const& braces, ExpressionSymbolMap& symbol_map) {
      LexerSpan span = braces;
      LocatorInfo locator = braces;
      if(span.empty()) {
        return ParseError{
          .position = braces.index(),
          .message = "Expected statements and expression in block."
        };
      }
      std::vector<located_output::Command> commands;
      while(true) {
        auto next_semi = find_if(span.begin(), span.end(), [&](lex_archive::Term const& term) {
          return term.holds_symbol() && symbol_map(term.get_symbol().symbol_index) == ExpressionSymbol::semicolon;
        });
        if(next_semi == span.end()) break;
        LexerSpan command_span{span.begin(), next_semi};
        if(command_span.empty()) continue; //ignore empty statements
        auto next_command_result = parse_statement(locator, command_span, next_semi + 1 == span.end(), symbol_map);
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
      auto expr_result = parse_expression(locator, span, symbol_map);
      if(expr_result.holds_error()) return std::move(expr_result);
      return located_output::Expression{located_output::Block{
        .statements = std::move(commands),
        .value = std::move(expr_result.get_value()),
        .position = position
      }};
    }
    /*mdb::Result<located_output::Expression, ParseError> parse_lexed_impl(LocatorInfo locator, LexerSpan span) {
      LexerSpan span = term.get_parenthesized_expression();
      LocatorInfo locator = term.get_parenthesized_expression();
      return parse_expression(locator, span);
    }*/
    lex_output::archive_part::SpanTerm::ConstIterator get_iterator(lex_archive_index::Term parent, std::uint64_t index, lex_output::archive_root::Term const& term) {
      if(auto* parens = term[parent].get_if_parenthesized_expression()) {
        return parens->body.begin() + index;
      } else if(auto* bracket = term[parent].get_if_bracket_expression()) {
        return bracket->body.begin() + index;
      } else {
        return term[parent].get_brace_expression().body.begin() + index;
      }
    }
    LocatorInfo get_locator(lex_output::archive_part::Term const& term) {
      if(auto* parens = term.get_if_parenthesized_expression()) {
        return *parens;
      } else if(auto* bracket = term.get_if_bracket_expression()) {
        return *bracket;
      } else {
        return term.get_brace_expression();
      }
    }
  }
  mdb::Result<located_output::Expression, ParseError> parse_lexed(lex_output::archive_root::Term const& term, LexerSpanIndex index, ExpressionSymbolMap map) {
    LexerSpan span{
      get_iterator(index.parent, index.begin_index, term),
      get_iterator(index.parent, index.end_index, term)
    };
    LocatorInfo locator = get_locator(term[index.parent]);
    return parse_expression(locator, span, map);
  }
  mdb::Result<located_output::Expression, ParseError> parse_lexed(lex_output::archive_root::Term const& term, ExpressionSymbolMap map) {
    LexerSpan span = term.root().get_parenthesized_expression();
    LocatorInfo locator = term.root().get_parenthesized_expression();
    return parse_expression(locator, span, map);
  }

}
