#include "standard_expression_parser.hpp"
#include "expression_folding_utility.hpp"

namespace standard_parser {
  namespace {
    struct expression_folder {
      using term_t = flat_parser_output::term;
      using binop_t = flat_parser_output::binop;
      using left_unop_t = flat_parser_output::left_unop;
      using right_unop_t = flat_parser_output::right_unop;
      using fold_result = folded_parser_output::any;
      fold_result process_term(term_t term) {
        return std::move(*term.data).visit_upon(utility::overloaded{
          [&](std::string_view&& id) -> fold_result {
            return folded_parser_output::id_t{id};
          },
          [&](std::vector<std::variant<term_t, binop_t, left_unop_t, right_unop_t> >&& vec) -> fold_result {
            return fold_flat_expression(*this, std::move(vec));
          }
        });
      }
      fold_result process_binary(fold_result lhs, binop_t op, fold_result rhs) {
        return folded_parser_output::binop{
          op,
          std::move(lhs),
          std::move(rhs)
        };
      }
      fold_result process_left_unary(left_unop_t op, fold_result rhs) {
        return folded_parser_output::abstraction{
          op.arg_name,
          std::move(rhs)
        };
      }
      fold_result process_right_unary(fold_result lhs, right_unop_t op) {
        std::terminate(); //there are no right unops
      }
      bool associate_left(binop_t lhs, binop_t rhs) {
        if((int)lhs > (int)rhs) return true;
        if((int)lhs < (int)rhs) return false;
        else if(lhs == binop_t::arrow) return false;
        return true;
      }
      bool associate_left(binop_t lhs, right_unop_t rhs) {
        std::terminate();
      }
      bool associate_left(left_unop_t lhs, binop_t rhs) {
        return false;
      }
      bool associate_left(left_unop_t lhs, right_unop_t rhs) {
        std::terminate();
      }
    };
    parse::parsing_routine<flat_parser_output::left_unop> parse_lambda() {
      using namespace parse;
      using namespace flat_parser_output;
      co_await whitespaces;
      co_await tag{"\\"};
      co_await whitespaces;
      auto name = (co_await capture{optional{parse_identifier}}).second;
      co_await whitespaces;
      co_await tag{"."};
      co_return left_unop{name};
    }
    parse::parsing_routine<flat_parser_output::binop> parse_binop() {
      using namespace parse;
      using namespace flat_parser_output;
      co_await whitespaces;
      //if(co_await optional{tag{"="}}) co_return binop::equals;
      //if(co_await optional{tag{"->"}}) co_return binop::arrow;
      //if(co_await optional{tag{":"}}) co_return binop::is_a;
      co_return binop::superposition;
    }
    parse::parsing_routine<flat_parser_output::term> parse_flat_expression();
    parse::parsing_routine<flat_parser_output::term> parse_term() {
      using namespace parse;
      using namespace flat_parser_output;
      co_await whitespaces;
      co_await error_override{"expected term"};
      if(auto id = co_await optional{parse_identifier}) {
        co_return term{*id};
      } else {
        co_await tag{"("};
        co_await cut;
        flat_parser_output::term first = co_await parse_flat_expression();
        co_await whitespaces;
        co_await tag{")"};
        co_return std::move(first);
      }
    }
    parse::parsing_routine<flat_parser_output::term::compound> parse_extension() {
      using namespace parse;
      using namespace flat_parser_output;
      flat_parser_output::term::compound ret;
      ret.push_back(co_await parse_binop());
      while(auto l = co_await optional{parse_lambda()}) {
        ret.push_back(*l);
      }
      ret.push_back(co_await parse_term());
      co_return std::move(ret);
    }
    parse::parsing_routine<flat_parser_output::term> parse_flat_expression() {
      using namespace parse;
      using namespace flat_parser_output;
      flat_parser_output::term::compound ret;
      while(auto l = co_await optional{parse_lambda()}) {
        ret.push_back(*l);
      }
      ret.push_back(co_await parse_term());
      while(auto continuation = co_await optional{parse_extension()}) {
        ret.insert(ret.end(), continuation->begin(), continuation->end());
      }
      co_return term{std::move(ret)};
    }
  }
  parse::parse_result::any<folded_parser_output::any> parse_expression(std::string_view input) {
    using namespace parse;
    return fmap{parse_flat_expression(), [](flat_parser_output::term output){
      return expression_folder{}.process_term(std::move(output));
    }}(input);
  };
}
