#include <fstream>
#include "User/interactive_environment.hpp"
#include "User/debug_format.hpp"
#include "Utility/vector_utility.hpp"
#include "Module/execute.hpp"
#include "Module/core_debug_print.hpp"
#include "Module/store.hpp"
#include "NewExpression/arena_utility.hpp"
#include "Pipeline/standard_compiler_context.hpp"

void debug_print_expr(new_expression::Arena& arena, new_expression::WeakExpression expr) {
  std::cout << user::raw_format(arena, expr) << "\n";
}
void debug_print_expr(new_expression::Arena& arena, new_expression::OwnedExpression const& expr) {
  std::cout << user::raw_format(arena, expr) << "\n";
}
std::uint64_t exponent(std::uint64_t base, std::uint64_t power) {
  std::uint64_t ret = 1;
  std::uint64_t pow = 1;
  while(pow <= power) {
    if(pow & power) {
      ret *= base;
    }
    base *= base;
    pow *= 2;
  }
  return ret;
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

struct NamedModule {
  expr_module::Core core;
  std::vector<std::string> names;
};

int main(int argc, char** argv) {
  std::vector<NamedModule> modules_loaded;

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
  {
    new_expression::Arena arena;
    {
      solver::BasicContext context(arena);
      auto exports = mdb::make_vector<new_expression::OwnedExpression>(
        arena.copy(context.primitives.type),
        arena.copy(context.primitives.arrow)
      );
      std::cout << debug_format(expr_module::store({
        .arena = arena,
        .rule_collector = context.rule_collector,
        .type_collector = context.type_collector,
        .get_import_index_of = [&](new_expression::WeakExpression expr) -> std::optional<std::uint32_t> {
          if(expr == context.primitives.arrow) {
            return 0;
          } else {
            return std::nullopt;
          }
        },
        .get_import_size = []() -> std::uint32_t { return 1; }
      }, {
        .exports = std::move(exports)
      })) << "\n";
    }
    arena.clear_orphaned_expressions();
    if(!arena.empty()) {
      std::cout << "Non-empty arena!\n";
      arena.debug_dump();
    }
  }
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
    if(line == "clear") {
      modules_loaded.clear();
      continue;
    }
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
    new_expression::Arena arena;
    {
      /*interactive::Environment environment(arena);
      auto& context = environment.context();

      auto mul_u64 = environment.declare("mul", "U64 -> U64 -> U64");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(mul_u64),
          .body = {
            .args_captured = 2,
            .steps = mdb::make_vector<new_expression::PatternStep>(
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 0,
                .expected_type = environment.u64()->type_index
              },
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 1,
                .expected_type = environment.u64()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return environment.u64()->make_expression(
                environment.u64()->read_data(arena.get_data(inputs[0])) * environment.u64()->read_data(arena.get_data(inputs[1]))
            );
          }
        }
      });
      auto sub_u64 = environment.declare("sub", "U64 -> U64 -> U64");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(sub_u64),
          .body = {
            .args_captured = 2,
            .steps = mdb::make_vector<new_expression::PatternStep>(
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 0,
                .expected_type = environment.u64()->type_index
              },
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 1,
                .expected_type = environment.u64()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return environment.u64()->make_expression(
                environment.u64()->read_data(arena.get_data(inputs[0])) - environment.u64()->read_data(arena.get_data(inputs[1]))
            );
          }
        }
      });
      auto exp_u64 = environment.declare("exp", "U64 -> U64 -> U64");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(exp_u64),
          .body = {
            .args_captured = 2,
            .steps = mdb::make_vector<new_expression::PatternStep>(
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 0,
                .expected_type = environment.u64()->type_index
              },
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 1,
                .expected_type = environment.u64()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return environment.u64()->make_expression(
                exponent(environment.u64()->read_data(arena.get_data(inputs[0])), environment.u64()->read_data(arena.get_data(inputs[1])))
            );
          }
        }
      });
      auto len = environment.declare("len", "String -> U64");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(len),
          .body = {
            .args_captured = 1,
            .steps = mdb::make_vector<new_expression::PatternStep>(
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 0,
                .expected_type = environment.str()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return environment.u64()->make_expression(
                environment.str()->read_data(arena.get_data(inputs[0])).size()
            );
          }
        }
      });*/

/*
      auto substr = environment.declare("substr", "String -> U64 -> U64 -> String");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(substr),
          .body = {
            .args_captured = 3,
            .steps = mdb::make_vector<new_expression::PatternStep>(
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 0,
                .expected_type = environment.str()->type_index
              },
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 1,
                .expected_type = environment.u64()->type_index
              },
              new_expression::PullArgument{},
              new_expression::DataCheck{
                .capture_index = 2,
                .expected_type = environment.u64()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return environment.str()->make_expression(
                environment.str()->read_data(arena.get_data(inputs[0])).substr(
                  environment.u64()->read_data(arena.get_data(inputs[1])),
                  environment.u64()->read_data(arena.get_data(inputs[2]))
                )
            );
          }
        }
      });*/

      pipeline::compile::StandardCompilerContext context(arena);
      auto module_info = context.create_module_primitives();
      auto possible_module_imports = mdb::make_vector<new_expression::OwnedExpression>(
        arena.copy(context.context.primitives.type),
        arena.copy(context.context.primitives.arrow),
        arena.copy(context.context.primitives.constant),
        arena.copy(context.context.primitives.id),
        arena.copy(context.vec_type),
        arena.copy(context.empty_vec),
        arena.copy(context.cons_vec),
        arena.copy(context.u64->type),
        arena.copy(context.str->type)
      );

      /*for(auto const& mod : modules_loaded) {
        auto eval_result = execute(
          {
            .arena = arena,
            .rule_collector = context.context.rule_collector,
            .type_collector = context.context.type_collector
          },
          mod.core,
          {
            .value_imports = mdb::make_vector(
              arena.copy(context.context.primitives.type),
              arena.copy(context.context.primitives.arrow),
              arena.copy(context.context.primitives.constant),
              arena.copy(context.context.primitives.id),
              arena.copy(vec_type),
              arena.copy(str->type)
            )
          }
        );
      }*/

      {
        new_expression::WeakKeyMap<std::string> externals_to_names(arena);
        externals_to_names.set(context.context.primitives.type, "Type");
        externals_to_names.set(context.context.primitives.arrow, "arrow");
        externals_to_names.set(context.u64->type, "U64");
        externals_to_names.set(context.str->type, "String");
        externals_to_names.set(context.vec_type, "Vector");
        externals_to_names.set(context.empty_vec, "empty_vec");
        externals_to_names.set(context.cons_vec, "cons_vec");
        externals_to_names.set(module_info.module_type, "Module");
        externals_to_names.set(module_info.module_ctor, "module");
        externals_to_names.set(module_info.module_entry_type, "ModuleEntry");
        externals_to_names.set(module_info.module_entry_ctor, "module_entry");

        auto result = full_compile(arena, source, context.combined_context());
        if(auto* value = result.get_if_value()) {
          value->report_errors_to(std::cout, {arena});
          auto namer = [&](std::ostream& o, new_expression::WeakExpression expr) {
            if(auto name = value->get_explicit_name_of(expr)) {
              o << *name;
            } else if(externals_to_names.contains(expr)) {
              o << externals_to_names.at(expr);
            } else if(arena.holds_declaration(expr)) {
              o << "decl_" << expr.data();
            } else if(arena.holds_axiom(expr)) {
              o << "ax_" << expr.data();
            } else {
              o << "???";
            }
          };
          std::cout << "RAW TYPE AND VALUE:\n";
          std::cout << user::raw_format(arena, value->result.value, namer) << "\n";
          std::cout << user::raw_format(arena, value->result.type, namer) << "\n";
          std::cout << "REDUCED TYPE AND VALUE:\n";
          new_expression::EvaluationContext ctx{arena, context.context.rule_collector};
          value->result.value = ctx.reduce(std::move(value->result.value));
          value->result.type = ctx.reduce(std::move(value->result.type));
          std::cout << user::raw_format(arena, value->result.value, namer) << "\n";
          std::cout << user::raw_format(arena, value->result.type, namer) << "\n";
          if(value->is_okay() && value->result.type == module_info.module_type) {
            std::cout << "Module!\n";
            new_expression::WeakExpression module_head = unfold(arena, value->result.value).args[0];
            std::vector<std::pair<std::string, new_expression::WeakExpression> > entries;
            while(true) {
              auto unfolded = unfold(arena, module_head);
              if(unfolded.args.size() != 3) break;
              auto entry = unfolded.args[1]; //head;
              auto entry_unfolded = unfold(arena, entry);
              if(entry_unfolded.args.size() != 3) break; //???
              auto str = context.str->read_data(arena.get_data(entry_unfolded.args[1])).get_string();
              auto value = entry_unfolded.args[2];
              module_head = unfolded.args[2];
              entries.emplace_back(std::string{str}, value);
            }
            for(auto const& entry : entries) {
              std::cout << "\"" << entry.first << "\": " << user::raw_format(arena, entry.second, namer) << "\n";
            }
            auto exports = mdb::map([&](auto const& entry) {
              return arena.copy(entry.second);
            }, entries);
            auto core = expr_module::store({
              .arena = arena,
              .rule_collector = context.context.rule_collector,
              .type_collector = context.context.type_collector,
              .get_import_index_of = [&](new_expression::WeakExpression expr) -> std::optional<std::uint32_t> {
                if(expr == context.context.primitives.type) {
                  return 0;
                } else if(expr == context.context.primitives.arrow) {
                  return 1;
                } else if(expr == context.context.primitives.constant) {
                  return 2;
                } else if(expr == context.context.primitives.id) {
                  return 3;
                } else if(expr == context.vec_type) {
                  return 4;
                } else {
                  return std::nullopt;
                }
              },
              .get_import_size = []() -> std::uint32_t { return 5; }
            }, {
              .exports = std::move(exports)
            });
            std::cout << debug_format(core) << "\n";
            modules_loaded.push_back({
              .core = std::move(core),
              .names = mdb::map([&](auto&& entry) {
                return std::move(entry.first);
              }, std::move(entries))
            });
          }
          /*auto exports = mdb::make_vector<new_expression::OwnedExpression>(
            arena.copy(value->result.value)
          );
          std::cout << debug_format(expr_module::store({
            .arena = arena,
            .rule_collector = context.rule_collector,
            .type_collector = context.type_collector,
            .get_import_index_of = [&](new_expression::WeakExpression expr) -> std::optional<std::uint32_t> {
              if(expr == environment.context().primitives.type) {
                return 0;
              } else if(expr == environment.context().primitives.arrow) {
                return 1;
              } else if(expr == environment.context().primitives.constant) {
                return 2;
              } else if(expr == environment.context().primitives.id) {
                return 3;
              } else {
                return std::nullopt;
              }
            },
            .get_import_size = []() -> std::uint32_t { return 4; }
          }, {
            .exports = std::move(exports)
          })) << "\n";*/



          destroy_from_arena(arena, *value);


        } else {
          std::cout << result.get_error() << "\n";
        }
      }
      destroy_from_arena(arena, module_info);
      //destroy_from_arena(arena, mul_u64, sub_u64, exp_u64, len, substr, module_entry_ctor, module_ctor, module_type);
    }
    arena.clear_orphaned_expressions();
    if(!arena.empty()) {
      std::cout << "Non-empty arena!\n";
      arena.debug_dump();
    }
  }
  return 0;
}

#endif
