#include "untyped_repl.hpp"
#include <sstream>
#include <fstream>
using namespace expressionz::standard;

namespace untyped_repl {
  void parsed_rule::execute(full_context& context) {
    standard_compiler::name_context local = context.names;
    std::size_t name_index = 0;
    for(auto n : standard_compiler::find_unbound_names(pattern, context.names)) {
      local.names.insert(std::make_pair(n, (standard_expression)expressionz::argument(name_index++)));
    }
    auto pattern_compiled = standard_compiler::compile(std::move(pattern), local);
    if(auto* err = std::get_if<std::string>(&pattern_compiled.as_variant)) {
      if(context.output) *context.output << "Compilation of pattern failed: " << *err << "\n";
    } else {
      auto& pattern = std::get<expressionz::standard::standard_expression>(pattern_compiled.as_variant);
      auto replacement_compiled = standard_compiler::compile(std::move(replacement), local);
      if(auto* err = std::get_if<std::string>(&replacement_compiled.as_variant)) {
        if(context.output) *context.output << "Compilation of replacement failed: " << *err << "\n";
      } else {
        auto& replacement = std::get<expressionz::standard::standard_expression>(replacement_compiled.as_variant);
        try {
          auto [decl, rule] = create_pattern_replacement_rule(pattern, replacement, context.expr);
          if(context.output) *context.output << "Rule created.\n";
          context.expr.add_rule(decl, std::move(rule));
        } catch (std::runtime_error& err) {
          if(context.output) *context.output << "Failed: " << err.what() << "\n";
        }
      }
    }
  }
  void parsed_declaration::execute(full_context& context) {
    std::string key(name);
    if(context.names.names.contains(key)) {
      if(context.output) *context.output << "Name " << key << " is already in use.\n";
    } else {
      auto decl = context.expr.register_declaration({key});
      context.names.names.insert(std::make_pair(key, (standard_expression)decl));
      if(context.output) *context.output << "Created declaration " << key << "\n";
    }
  }
  void parsed_axiom::execute(full_context& context) {
    std::string key(name);
    if(context.names.names.contains(key)) {
      if(context.output) *context.output << "Name " << key << " is already in use.\n";
    } else {
      auto ax = context.expr.register_axiom({key});
      context.names.names.insert(std::make_pair(key, (standard_expression)ax));
      if(context.output) *context.output << "Created axiom " << key << "\n";
    }
  }
  void parsed_evaluator::execute(full_context& context) {
    auto expr_compiled = standard_compiler::compile(std::move(expr), context.names);
    if(auto* err = std::get_if<std::string>(&expr_compiled.as_variant)) {
      if(context.output) *context.output << "Compilation of term failed: " << *err << "\n";
    } else {
      auto& e = std::get<expressionz::standard::standard_expression>(expr_compiled.as_variant);
      if(context.output){ run_routine(simple_output, e, context.expr, *context.output); *context.output << "\n"; }
      auto [e_final, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
        if(context.output) {
          exec(simple_output(outer, context.expr, *context.output));
          *context.output << "\n";
        }
      }, simplify_total, e, context.expr);
      if(set_name) {
        context.names.names.insert_or_assign(std::string(*set_name), e_final);
      }
    }
  }
  std::optional<std::string> parsed_test::execute(full_context& context) {
    std::stringstream error_output;
    auto expr_compiled = standard_compiler::compile(std::move(expr), context.names);
    auto result_compiled = standard_compiler::compile(std::move(result), context.names);
    if(auto* err = std::get_if<std::string>(&expr_compiled.as_variant)) {
      error_output << "Compilation of expression failed: " << *err << "\n";
      return error_output.str();
    } else if(auto* err = std::get_if<std::string>(&result_compiled.as_variant)) {
      error_output << "Compilation of result failed: " << *err << "\n";
      return error_output.str();
    } else {
      auto& e = std::get<expressionz::standard::standard_expression>(expr_compiled.as_variant);
      auto r = std::get<expressionz::standard::standard_expression>(result_compiled.as_variant);
      error_output << "Expression: "; run_routine(simple_output, e, context.expr, error_output); error_output << "\n";
      error_output << "Expected result: "; run_routine(simple_output, r, context.expr, error_output); error_output << "\n";
      error_output << "Simplification cascade:\n";
      auto [e_final, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
        error_output << "\t";
        exec(simple_output(outer, context.expr, error_output));
        error_output << "\n";
      }, simplify_total, e, context.expr);
      if(!expressionz::__deep_literal_compare(e_final, r)) {
        return error_output.str();
      } else {
        return std::nullopt;
      }
    }
  }
  namespace {
    parse::parsing_routine<parsed_rule> parse_rule() {
      using namespace parse;
      co_await with_traceback{"rule"};
      co_await tag{"rule"};
      co_await cut;
      co_await whitespace;
      auto pat = co_await standard_parser::parse_expression;
      co_await whitespaces;
      co_await tag{":="};
      auto replacement = co_await standard_parser::parse_expression;
      co_return parsed_rule{std::move(pat), std::move(replacement)};
    }
    parse::parsing_routine<parsed_axiom> parse_axiom() {
      using namespace parse;
      co_await with_traceback{"axiom"};
      co_await tag{"axiom"};
      co_await cut;
      co_await whitespaces;
      auto name = co_await parse_identifier;
      co_return parsed_axiom{name};
    };
    parse::parsing_routine<parsed_declaration> parse_declaration() {
      using namespace parse;
      co_await with_traceback{"declare"};
      co_await tag{"declare"};
      co_await whitespaces;
      auto name = co_await parse_identifier;
      co_return parsed_declaration{name};
    };
    parse::parsing_routine<parsed_evaluator> parse_set_evaluator() {
      using namespace parse;
      co_await with_traceback{"alias"};
      co_await tag{"alias"};
      co_await cut;
      co_await whitespaces;
      auto name = co_await parse_identifier;
      co_await whitespaces;
      co_await tag{":="};
      auto expr = co_await standard_parser::parse_expression;
      co_return parsed_evaluator{std::move(expr), std::move(name)};
    }
    parse::parsing_routine<parsed_evaluator> parse_evaluator() {
      using namespace parse;
      auto expr = co_await standard_parser::parse_expression;
      co_return parsed_evaluator{std::move(expr)};
    }
    parse::parsing_routine<parsed_test> parse_test() {
      using namespace parse;
      co_await with_traceback{"test"};
      co_await tag{"test"};
      co_await cut;
      co_await whitespace;
      auto expr = co_await standard_parser::parse_expression;
      co_await whitespaces;
      co_await tag{";"};
      co_await whitespaces;
      co_await tag{"expect"};
      co_await whitespace;
      auto result = co_await standard_parser::parse_expression;
      co_return parsed_test{std::move(expr), std::move(result)};
    };
  }
  parse::parsing_routine<parsed_command> parse_command() {
    using namespace parse;
    co_await whitespaces;
    if(auto rule = co_await optional{parse_rule()}) co_return std::move(*rule);
    if(auto axiom = co_await optional{parse_axiom()}) co_return std::move(*axiom);
    if(auto declaration = co_await optional{parse_declaration()}) co_return std::move(*declaration);
    if(auto evaluator = co_await optional{parse_set_evaluator()}) co_return std::move(*evaluator);
    if(auto test = co_await optional{parse_test()}) co_return std::move(*test);
    co_return co_await parse_evaluator();
  }
  parse::parsing_routine<parsed_command> parse_terminated_command() {
    using namespace parse;
    auto ret = co_await parse_command();
    co_await whitespaces;
    co_await tag{";"};
    co_return std::move(ret);
  }

  std::string load_file(std::string path) {
    std::ifstream input(path);
    if(!input) throw std::runtime_error("file " + path + " not found.");
    input.seekg(0, std::ios_base::end);
    auto length = input.tellg();
    input.seekg(0);
    std::string ret(length, ' ');
    input.read(&ret[0], length);
    return ret;
  }
}
