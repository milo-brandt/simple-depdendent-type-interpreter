#include <iostream>
#include <fstream>

#include "standard_expression_compiler.hpp"

using namespace expressionz::standard;
using namespace expressionz::coro;

struct full_context {
  standard_compiler::name_context names;
  context expr;
};
struct parsed_rule {
  standard_parser::folded_parser_output::any pattern;
  standard_parser::folded_parser_output::any replacement;
  void execute(full_context&);
};
struct parsed_declaration {
  std::string_view name;
  void execute(full_context&);
};
struct parsed_axiom {
  std::string_view name;
  void execute(full_context&);
};
struct parsed_evaluator {
  standard_parser::folded_parser_output::any expr;
  std::optional<std::string_view> set_name;
  void execute(full_context&);
};
void parsed_rule::execute(full_context& context) {
  standard_compiler::name_context local = context.names;
  std::size_t name_index = 0;
  for(auto n : standard_compiler::find_unbound_names(pattern, context.names)) {
    local.names.insert(std::make_pair(n, (standard_expression)expressionz::argument(name_index++)));
  }
  auto pattern_compiled = standard_compiler::compile(std::move(pattern), local);
  if(auto* err = std::get_if<std::string>(&pattern_compiled.as_variant)) {
    std::cout << "Compilation of pattern failed: " << *err << "\n";
  } else {
    auto& pattern = std::get<expressionz::standard::standard_expression>(pattern_compiled.as_variant);
    auto replacement_compiled = standard_compiler::compile(std::move(replacement), local);
    if(auto* err = std::get_if<std::string>(&replacement_compiled.as_variant)) {
      std::cout << "Compilation of replacement failed: " << *err << "\n";
    } else {
      auto& replacement = std::get<expressionz::standard::standard_expression>(replacement_compiled.as_variant);
      try {
        auto [decl, rule] = create_pattern_replacement_rule(pattern, replacement, context.expr);
        std::cout << "Rule created.\n";
        context.expr.add_rule(decl, std::move(rule));
      } catch (std::runtime_error& err) {
        std::cout << "Failed: " << err.what() << "\n";
      }
    }
  }
}
void parsed_declaration::execute(full_context& context) {
  std::string key(name);
  if(context.names.names.contains(key)) {
    std::cout << "Name " << key << " is already in use.\n";
  } else {
    auto decl = context.expr.register_declaration({key});
    context.names.names.insert(std::make_pair(key, (standard_expression)decl));
    std::cout << "Created declaration " << key << "\n";
  }
}
void parsed_axiom::execute(full_context& context) {
  std::string key(name);
  if(context.names.names.contains(key)) {
    std::cout << "Name " << key << " is already in use.\n";
  } else {
    auto ax = context.expr.register_axiom({key});
    context.names.names.insert(std::make_pair(key, (standard_expression)ax));
    std::cout << "Created axiom " << key << "\n";
  }
}
void parsed_evaluator::execute(full_context& context) {
  auto expr_compiled = standard_compiler::compile(std::move(expr), context.names);
  if(auto* err = std::get_if<std::string>(&expr_compiled.as_variant)) {
    std::cout << "Compilation of pattern failed: " << *err << "\n";
  } else {
    auto& e = std::get<expressionz::standard::standard_expression>(expr_compiled.as_variant);
    run_routine(simple_output, e, context.expr, std::cout); std::cout << "\n";
    auto [e_final, _] = run_routine_with_logger_routine([&](auto& exec, auto outer, auto inner){
      exec(simple_output(outer, context.expr, std::cout));
      std::cout << "\n";
    }, simplify_total, e, context.expr);
    if(set_name) {
      context.names.names.insert_or_assign(std::string(*set_name), e_final);
    }
  }
}
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
using parsed_command = std::variant<parsed_rule, parsed_axiom, parsed_declaration, parsed_evaluator>;
parse::parsing_routine<parsed_command> parse_command() {
  using namespace parse;
  co_await whitespaces;
  if(auto rule = co_await optional{parse_rule()}) co_return std::move(*rule);
  if(auto axiom = co_await optional{parse_axiom()}) co_return std::move(*axiom);
  if(auto declaration = co_await optional{parse_declaration()}) co_return std::move(*declaration);
  if(auto evaluator = co_await optional{parse_set_evaluator()}) co_return std::move(*evaluator);
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

int main() {
  full_context ctx;
  try {
    auto preamble = load_file("source.txt");
    std::string_view remaining = preamble;
    while(true) {
      auto result = parse_terminated_command()(remaining);
      if(auto* err = std::get_if<0>(&result)) {
        std::cout << "Parsed until " << err->position << "\n";
        std::cout << "Error: " << err->reason << "\n";
        for(auto& [frame, pos] : err->stack) {
          std::cout << "While parsing: " << frame << "\n";
          std::cout << "At: " << pos << "\n";
        }
        break;
      } else if(auto* err = std::get_if<1>(&result)) {
        std::cout << "Parsed until " << err->position << "\n";
        std::cout << "Failure: " << err->reason << "\n";
        for(auto& [frame, pos] : err->stack) {
          std::cout << "While parsing: " << frame << "\n";
          std::cout << "At: " << pos << "\n";
        }
        break;
      } else {
        auto& success = std::get<2>(result);
        std::visit([&](auto&& x){ return std::move(x).execute(ctx); }, std::move(success.value));
        remaining = success.position;
      }
    }
  } catch(std::runtime_error& e) {
    std::cout << e.what() << "\n";
  }
  while(true){
    std::string input;
    std::getline(std::cin, input);
    if(input == "quit") return 0;
    auto result = parse_command()(input);
    if(auto* err = std::get_if<0>(&result)) {
      std::cout << "Parsed until " << err->position << "\n";
      std::cout << "Error: " << err->reason << "\n";
      for(auto& [frame, pos] : err->stack) {
        std::cout << "While parsing: " << frame << "\n";
        std::cout << "At: " << pos << "\n";
      }
    } else if(auto* err = std::get_if<1>(&result)) {
      std::cout << "Parsed until " << err->position << "\n";
      std::cout << "Failure: " << err->reason << "\n";
      for(auto& [frame, pos] : err->stack) {
        std::cout << "While parsing: " << frame << "\n";
        std::cout << "At: " << pos << "\n";
      }
    } else {
      std::visit([&](auto&& x){ return std::move(x).execute(ctx); }, std::move(std::get<2>(result).value));
    }

  }
}
