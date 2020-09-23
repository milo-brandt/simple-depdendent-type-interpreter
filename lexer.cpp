#include "lexer.hpp"
#include <cctype>
#include "Templates/TemplateUtility.h"

namespace lexer{
  void lexer::add_keyword(std::string keyword){
    keywords.insert(keyword);
  }
  void lexer::add_symbol(std::string symbol){
    symbols.insert(std::make_pair(std::move(symbol), symbol_interpretation{pure_symbol{}}));
  }
  void lexer::add_string_delimeter(std::string open, std::string close){
    symbols.insert(std::make_pair(std::move(open), symbol_interpretation{open_str_delim{std::move(close)}}));
  }
  void lexer::add_delimeter(std::string open, std::string close){
    symbols.insert(std::make_pair(std::move(open), symbol_interpretation{open_delim{close}}));
    symbols.insert(std::make_pair(std::move(close), symbol_interpretation{close_delim{}}));
  }
  struct lexer::detail{
    enum class symbol_kind{
      identifier,
      numeric,
      symbol,
      whitespace
    };
    static symbol_kind classify(char c){
      if(std::isspace(c)) return symbol_kind::whitespace;
      if(std::isalpha(c) || c == '_') return symbol_kind::identifier;
      if(std::isdigit(c)) return symbol_kind::numeric;
      return symbol_kind::symbol;
    }
    static bool can_be_part_of_identifier(char c){
      return std::isalnum(c) || c == '_';
    }
    static bool can_be_part_of_numeric(char c){
      return std::isdigit(c);
    }
    static bool can_be_part_of_symbol(char c){
      return classify(c) == symbol_kind::symbol;
    }
    struct delimeter_stack_frame{
      std::string_view open;
      std::string close_delim;
    };
    static std::string to_string(std::string_view s){
      return std::string{s.begin(), s.end()};
    }
  };
  output lexer::lex(std::string_view in){
    std::vector<output> data_stack;
    data_stack.emplace_back();
    std::vector<detail::delimeter_stack_frame> stack;
    for(const char* pos = in.begin(); pos != in.end(); ++pos){
      switch(detail::classify(*pos)){
        case detail::symbol_kind::identifier:
        {
          const char* start = pos;
          do{ ++pos; } while(pos != in.end() && detail::can_be_part_of_identifier(*pos));
          std::string_view str = {start, std::size_t(pos - start)};
          if(keywords.find(detail::to_string(str)) != keywords.end()) data_stack.back().push_back(output_parts::symbol{{start, std::size_t(pos - start)}});
          else data_stack.back().push_back(output_parts::identifier{{start, std::size_t(pos - start)}});
          break;
        }
        case detail::symbol_kind::numeric:
        {
          const char* start = pos;
          do{ ++pos; } while(pos != in.end() && detail::can_be_part_of_numeric(*pos));
          data_stack.back().push_back(output_parts::identifier{{start, std::size_t(pos - start)}});
          break;
        }
        case detail::symbol_kind::symbol:
        {
          const char* start = pos;
          do{ ++pos; } while(pos != in.end() && detail::can_be_part_of_symbol(*pos));
          std::string_view str = {start, std::size_t(pos - start)};
          auto symbol_pos = symbols.lower_bound(detail::to_string(str));
          if(symbol_pos == symbols.end() || symbol_pos->first != str){
            if(symbol_pos == symbols.begin()) std::terminate(); //throw std::runtime_error("unrecognized symbol \"" + detail::to_string(str) + "\"");
            --symbol_pos;
          }
          //if(!str.starts_with(symbol_pos->first)) std::terminate(); //throw std::runtime_error("unrecognized symbol \"" + detail::to_string(str) + "\"");
          pos = start + symbol_pos->first.size();
          str = std::string_view{start, std::size_t(pos - start)};
          std::visit(utility::overloaded{
            [&](pure_symbol){
              data_stack.back().push_back(output_parts::symbol{str});
            },
            [&](open_delim const& open){
              stack.push_back(detail::delimeter_stack_frame{str, open.close});
              data_stack.emplace_back();
            },
            [&](close_delim){
              if(stack.empty()) std::terminate(); //throw std::runtime_error("nothing to close");
              if(str != stack.back().close_delim) std::terminate(); //throw std::runtime_error("mismatched delimeters");
              output inner = std::move(data_stack.back());
              data_stack.pop_back();
              data_stack.back().push_back(output_parts::delimeter_expression{stack.back().open, std::move(inner), str});
              stack.pop_back();
            },
            [&](open_str_delim const& str_delim){
              std::size_t end = in.find(str_delim.close, pos - in.begin());
              if(end == std::string::npos) std::terminate(); //throw std::runtime_error("unterminated string literal");
              const char* end_pos = in.begin();
              const char* final_pos = end_pos + str_delim.close.size();
              data_stack.back().push_back(output_parts::string_literal{str, {pos, std::size_t(end_pos-pos)}, {end_pos, std::size_t(final_pos-end_pos)}});
            }
          }, symbol_pos->second);
          break;
        }
        case detail::symbol_kind::whitespace: break;
      }
    }
    if(!stack.empty()) std::terminate(); //throw std::runtime_error("unclosed delimeter");
    return std::move(data_stack.back());
  }

  namespace{
    void output_to_stream(std::ostream& o, output const& output, std::string prefix){
      o << prefix << "{\n";
      std::string inner_prefix = prefix + " ";
      for(auto part : output){
        part->visit_upon(utility::overloaded{
          [&](output_parts::identifier const& id){ o << inner_prefix << "id(" << id.span << ")\n"; },
          [&](output_parts::symbol const& id){ o << inner_prefix << "sym(" << id.span << ")\n"; },
          [&](output_parts::numeric_literal const& lit){ o << inner_prefix << "num(" << lit.value << ")\n"; },
          [&](output_parts::string_literal const& lit){ o << inner_prefix << "str(" << lit.inner << ")\n"; },
          [&](output_parts::delimeter_expression const& delim){ o << inner_prefix << "delimeter_expression(" << delim.open << "..." << delim.close << ")\n"; output_to_stream(o, delim.inner, inner_prefix); }
        });
      }
      o << prefix << "}\n";
    }
  }
  std::ostream& operator<<(std::ostream& o, output const& output){
    output_to_stream(o, output, "");
    return o;
  }

}
