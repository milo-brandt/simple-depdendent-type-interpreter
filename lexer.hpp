#ifndef lexer_h
#define lexer_h

#include <string>
#include <variant>
#include <map>
#include <unordered_set>
#include <exception>
#include <iostream>
#include "Templates/indirect_variant.h"

namespace lexer{
  namespace output_parts{
    struct identifier;
    struct symbol;
    struct delimeter_expression;
    struct numeric_literal;
    struct string_literal;
    using lex_tree = std::vector<indirect_variant<identifier, symbol, delimeter_expression, numeric_literal, string_literal> >;
    struct identifier{
      std::string_view span;
    };
    struct symbol{
      std::string_view span;
    };
    struct numeric_literal{
      long value;
    };
    struct string_literal{
      std::string_view open;
      std::string_view inner;
      std::string_view close;
    };
    struct delimeter_expression{
      std::string_view open;
      lex_tree inner;
      std::string_view close;
    };
  };
  using output = output_parts::lex_tree;

  class lexer{
    std::unordered_set<std::string> keywords;
    struct pure_symbol{};
    struct open_delim{ std::string close; };
    struct open_str_delim{ std::string close; };
    struct close_delim{};
    using symbol_interpretation = std::variant<pure_symbol, open_delim, open_str_delim, close_delim>;
    std::map<std::string, symbol_interpretation> symbols;
    struct detail;
  public:
    void add_keyword(std::string);
    void add_symbol(std::string);
    void add_string_delimeter(std::string open, std::string close);
    void add_delimeter(std::string open, std::string close);
    output lex(std::string_view);
  };

  std::ostream& operator<<(std::ostream&, output const&);
};

#endif
