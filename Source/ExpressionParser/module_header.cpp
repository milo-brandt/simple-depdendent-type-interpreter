#include "module_header.hpp"

namespace expression_parser {
  namespace {
    /*
      This code is copied from expression_generator.cpp. Should probably refactor - it's a useful utility
    */
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
    mdb::Result<ModuleResult, ParseError> parse_header(LocatorInfo const& locator, LexerSpan& span, HeaderSymbolMap& symbol_map) {
      std::vector<ImportInfo> imports;
      while(!span.empty() && span.begin()->holds_symbol() && symbol_map(span.begin()->get_symbol().symbol_index) == HeaderSymbol::import) {
        //parse an import.
        ++span.span_begin;
        std::vector<std::string> requested_names;
        bool request_all = false;
        if(span.empty()) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Expected name listing after 'import'."
          };
        }
        if(span.begin()->holds_symbol()) {
          if(symbol_map(span.begin()->get_symbol().symbol_index) == HeaderSymbol::star) {
            request_all = true;
            ++span.span_begin;
          } else {
            return ParseError{
              .position = span.span_begin->index(),
              .message = "Unexpected symbol in name listing of import statement."
            };
          }
        } else if(auto* braced = span.begin()->get_if_brace_expression()) {
          LexerSpan inner = *braced;
          bool first = true;
          while(!inner.empty()) {
            if(first) first = false;
            else {
              if(!inner.begin()->holds_symbol() || symbol_map(inner.begin()->get_symbol().symbol_index) != HeaderSymbol::comma) {
                return ParseError{
                  .position = inner.span_begin->index(),
                  .message = "Expected comma after import symbol name."
                };
              }
              ++inner.span_begin;
              if(inner.empty()) {
                return ParseError{
                  .position = (inner.span_begin - 1)->index(),
                  .message = "Expected symbol name after comma."
                };
              }
            }
            if(!inner.begin()->holds_word()) {
              return ParseError{
                .position = inner.span_begin->index(),
                .message = "Expected identifier in symbol name list."
              };
            }
            requested_names.emplace_back(inner.begin()->get_word().text);
            ++inner.span_begin;
          }
          ++span.span_begin;
        } else if(auto* word = span.begin()->get_if_word()) {
          requested_names.emplace_back(word->text);
          ++span.span_begin;
        } else {
          return ParseError{
            .position = span.span_begin->index(),
            .message = "Unexpected token in name listing of import statement."
          };
        }
        if(span.empty() || !span.begin()->holds_symbol() || symbol_map(span.begin()->get_symbol().symbol_index) != HeaderSymbol::from) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Expected keyword 'from' after name listing of import statement."
          };
        }
        ++span.span_begin;
        if(span.empty() || !span.begin()->holds_word()) return ParseError{
          .position = (span.span_begin - 1)->index(),
          .message = "Expected module name after 'from'."
        };
        std::string module_name{span.begin()->get_word().text};
        ++span.span_begin;
        if(span.empty() || !span.begin()->holds_symbol() || symbol_map(span.begin()->get_symbol().symbol_index) != HeaderSymbol::semicolon) {
          return ParseError{
            .position = (span.span_begin - 1)->index(),
            .message = "Expected semicolon after import statement."
          };
        }
        ++span.span_begin;
        imports.push_back({
          .module_name = std::move(module_name),
          .request_all = request_all,
          .requested_names = std::move(requested_names)
        });
      }
      return ModuleResult{
        .header = {
          .imports = std::move(imports)
        },
        .remaining = locator.to_span(span)
      };
    }
  }
  mdb::Result<ModuleResult, ParseError> parse_module_header(lex_output::archive_root::Term const& term, HeaderSymbolMap symbol_map) {
    LexerSpan span = term.root().get_parenthesized_expression();
    LocatorInfo locator = term.root().get_parenthesized_expression();
    return parse_header(locator, span, symbol_map);
  }
}
