#include "standard_expression_compiler.hpp"

namespace standard_compiler {
  namespace {
    void find_unbound_names_impl(std::unordered_set<std::string>& ret, standard_parser::folded_parser_output::any const& root, name_context const& ctx) {
      using namespace standard_parser;
      if(auto* id = root->get_if<folded_parser_output::id_t>()) {
        std::string key(id->id);
        if(!ctx.names.contains(key)) {
          ret.insert(std::move(key));
        }
      } else if(auto* binop = root->get_if<folded_parser_output::binop>()) {
        find_unbound_names_impl(ret, binop->lhs, ctx);
        find_unbound_names_impl(ret, binop->rhs, ctx);
      } else if(auto* abstract = root->get_if<folded_parser_output::abstraction>()) {
        name_context inner = ctx;
        std::string key(abstract->arg);
        inner.names.insert_or_assign(std::move(key), expressionz::argument(0));
        find_unbound_names_impl(ret, abstract->body, inner);
      }
    }
  }
  std::unordered_set<std::string> find_unbound_names(standard_parser::folded_parser_output::any const& root, name_context const& ctx) {
    std::unordered_set<std::string> ret;
    find_unbound_names_impl(ret, root, ctx);
    return ret;
  }
  namespace {
    result_routine<standard_expression, std::string> compile_impl(standard_parser::folded_parser_output::any const& root, name_context const& ctx) {
      using namespace standard_parser;
      if(auto* id = root->get_if<folded_parser_output::id_t>()) {
        std::string key(id->id);
        if(!ctx.names.contains(key)) co_yield "unrecognized id " + key;
        co_return ctx.names.at(key);
      } else if(auto* binop = root->get_if<folded_parser_output::binop>()) {
        //assume it's apply.
        auto f = co_await compile_impl(binop->lhs, ctx);
        auto x = co_await compile_impl(binop->rhs, ctx);
        co_return expressionz::apply(f, x);
      } else if(auto* abstract = root->get_if<folded_parser_output::abstraction>()) {
        name_context inner = ctx;
        for(auto& [key, value] : inner.names) {
          value = expressionz::deepen(1, value);
        }
        std::string key(abstract->arg);
        inner.names.insert_or_assign(std::move(key), expressionz::argument(0));
        co_return expressionz::abstract(co_await compile_impl(abstract->body, inner));
      }
      std::terminate();
    }
  }
  result<standard_expression, std::string> compile(standard_parser::folded_parser_output::any const& root, name_context const& ctx) {
    return compile_impl(root, ctx);
  }
}
