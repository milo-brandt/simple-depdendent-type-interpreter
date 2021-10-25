#include <fstream>
#include "User/interactive_environment.hpp"
#include "User/debug_format.hpp"
#include "Utility/vector_utility.hpp"


void debug_print_expr(new_expression::Arena& arena, new_expression::WeakExpression expr) {
  std::cout << user::raw_format(arena, expr) << "\n";
}
void debug_print_expr(new_expression::Arena& arena, new_expression::OwnedExpression const& expr) {
  std::cout << user::raw_format(arena, expr) << "\n";
}

#ifdef COMPILE_FOR_EMSCRIPTEN

#include <emscripten/bind.h>
#include <sstream>


std::string replace_newlines_with_br(std::string input) {
  while(true) { //this isn't very efficient - but not a bottleneck either
    auto next_new_line = input.find('\n');
    if(next_new_line == std::string::npos) return input;
    input.replace(next_new_line, 1, "<br/>");
  }
}

expression::interactive::Environment& get_last_environment() {
  static expression::interactive::Environment ret = setup_enviroment();
  return ret;
}
void reset_environment() {
  get_last_environment() = setup_enviroment();
}

std::string run_script(std::string script, bool reset) {
  std::stringstream ret;
  std::string_view source = script;
  if(reset) reset_environment();
  get_last_environment().debug_parse(source, ret);
  return replace_newlines_with_br(ret.str());
}
int main(int argc, char** argv) {
  get_last_environment();
}

using namespace emscripten;
EMSCRIPTEN_BINDINGS(my_module) {
    function("run_script", &run_script);
}

#else

int main(int argc, char** argv) {

  /*if(argc == 2) {
    std::ifstream f(argv[1]);
    if(!f) {
      std::cout << "Failed to read file \"" << argv[1] << "\"\n";
      return -1;
    } else {
      environment = setup_enviroment(); //clean environment
      std::string source_str;
      std::getline(f, source_str, '\0'); //just read the whole file - assuming no null characters in it
      std::string_view source = source_str;
      environment.debug_parse(source);
      return 0;
    }
  } else if(argc > 2) {
    std::cout << "The interpreter expects either a single file to run as an argument or no arguments to run in interactive mode.\n";
    return -1;
  }*/

  //interactive mode
  std::string last_line = "";
  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line.empty()) {
      if(last_line.empty()) continue;
      line = last_line;
    }
    while(line.back() == '\\') { //keep reading
      line.back() = '\n'; //replace backslash with new line
      std::string next_line;
      std::getline(std::cin, next_line);
      line += next_line;
    }
    last_line = line;
    if(line == "q") return 0;
    if(line.starts_with("file ")) {
      std::ifstream f(line.substr(5));
      if(!f) {
        std::cout << "Failed to read file \"" << line.substr(5) << "\"\n";
        continue;
      } else {
        std::string total;
        std::getline(f, total, '\0'); //just read the whole file - assuming no null characters in it :P
        std::cout << "Contents of file:\n" << total << "\n";
        line = std::move(total);
      }
    }
    std::string_view source = line;
    interactive::Environment environment;
    auto& arena = environment.arena();
    auto& context = environment.context();
    auto add_u64 = environment.declare("add", "U64 -> U64 -> U64");
    context.rule_collector.add_rule({
      .pattern = {
        .head = arena.copy(add_u64),
        .body = {
          .args_captured = 2,
          .steps = mdb::make_vector<new_expression::PatternStep>(
            new_expression::PullArgument{},
            new_expression::PatternMatch{
              .substitution = arena.argument(0),
              .expected_head = arena.copy(environment.u64_head()),
              .args_captured = 1
            },
            new_expression::DataCheck{
              .capture_index = 1,
              .expected_type = environment.u64()->type_index
            },
            new_expression::PullArgument{},
            new_expression::PatternMatch{
              .substitution = arena.argument(2),
              .expected_head = arena.copy(environment.u64_head()),
              .args_captured = 1
            },
            new_expression::DataCheck{
              .capture_index = 3,
              .expected_type = environment.u64()->type_index
            }
          )
        }
      },
      .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
        [&](std::span<new_expression::WeakExpression> inputs) {
          return arena.apply(
            arena.copy(environment.u64_head()),
            environment.u64()->make_expression(
              environment.u64()->read_data(arena.get_data(inputs[1])) + environment.u64()->read_data(arena.get_data(inputs[3]))
            )
          );
        }
      }
    });
    auto mul_u64 = environment.declare("mul", "U64 -> U64 -> U64");
    context.rule_collector.add_rule({
      .pattern = {
        .head = arena.copy(mul_u64),
        .body = {
          .args_captured = 2,
          .steps = mdb::make_vector<new_expression::PatternStep>(
            new_expression::PullArgument{},
            new_expression::PatternMatch{
              .substitution = arena.argument(0),
              .expected_head = arena.copy(environment.u64_head()),
              .args_captured = 1
            },
            new_expression::DataCheck{
              .capture_index = 1,
              .expected_type = environment.u64()->type_index
            },
            new_expression::PullArgument{},
            new_expression::PatternMatch{
              .substitution = arena.argument(2),
              .expected_head = arena.copy(environment.u64_head()),
              .args_captured = 1
            },
            new_expression::DataCheck{
              .capture_index = 3,
              .expected_type = environment.u64()->type_index
            }
          )
        }
      },
      .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
        [&](std::span<new_expression::WeakExpression> inputs) {
          return arena.apply(
            arena.copy(environment.u64_head()),
            environment.u64()->make_expression(
              environment.u64()->read_data(arena.get_data(inputs[1])) * environment.u64()->read_data(arena.get_data(inputs[3]))
            )
          );
        }
      }
    });

    environment.debug_parse(source);
  }
  return 0;
}

#endif
