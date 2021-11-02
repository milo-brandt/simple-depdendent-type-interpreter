#include <fstream>
#include "User/interactive_environment.hpp"
#include "User/debug_format.hpp"
#include "Utility/vector_utility.hpp"
#include "Module/execute.hpp"
#include "Module/core_debug_print.hpp"
#include "Module/store.hpp"
#include "NewExpression/arena_utility.hpp"
#include "Pipeline/standard_compiler_context.hpp"
#include "Pipeline/compile_stages.hpp"

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

struct ModuleInfo {
  std::unordered_map<std::string, new_expression::TypedValue> exported_terms;
  pipeline::compile::EvaluateInfo evaluate_info;
  std::string source;
  static constexpr auto part_info = mdb::parts::simple<3>;
};
struct ModuleLoadInfo {
  std::unordered_set<std::string> modules_loading;
  std::vector<std::string> module_loading_stack;
  std::unordered_map<std::string, ModuleInfo> module_info;
  static constexpr auto part_info = mdb::parts::simple<3>;
  bool locate_and_prepare_module(std::string module_name, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives);
  bool prepare_module(std::string module_name, std::string source, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives) {
    if(module_info.contains(module_name)) return true;
    if(modules_loading.contains(module_name)) {
      std::cout << "Cyclic dependency in modules. Backtrace:\n";
      bool found = false;
      for(auto const& name : module_loading_stack) {
        found = found || name == module_name;
        if(found) {
          std::cout << name << "\n";
        }
      }
      modules_loading.clear();
      module_loading_stack.clear();
      return false;
    }
    modules_loading.insert(module_name);
    module_loading_stack.push_back(module_name);
    auto parsed_module_result = bind(
      pipeline::compile::lex({context.arena, source}),
      [&](auto lexed) {
        return parse_module(std::move(lexed));
      });
    if(parsed_module_result.holds_error()) {
      std::cout << parsed_module_result.get_error() << "\n";
      return false;
    }
    auto& parsed_module = parsed_module_result.get_value();
    std::unordered_map<std::string, new_expression::TypedValue> names_to_values;
    new_expression::RAIIDestroyer destroyer{context.arena, names_to_values};
    for(auto const& entry : context.names_to_values) {
      names_to_values.insert(std::make_pair(entry.first, copy_on_arena(context.arena, entry.second)));
    }
    auto add_name = [&](std::string name, new_expression::TypedValue value) {
      if(names_to_values.contains(name)) {
        destroy_from_arena(context.arena, value);
        std::cout << "Duplicated name " << name << " in imports of " << module_name << ".\n";
        return false;
      } else {
        names_to_values.insert(std::make_pair(std::move(name), std::move(value)));
        return true;
      }
    };
    for(auto const& import_info : parsed_module.first.imports) {
      if(!locate_and_prepare_module(import_info.module_name, context, module_primitives)) return false;
      auto const& info = module_info.at(import_info.module_name);
      if(import_info.request_all) {
        for(auto const& entry : info.exported_terms) {
          if(!add_name(entry.first, copy_on_arena(context.arena, entry.second))) return false;
        }
      } else {
        for(auto const& request : import_info.requested_names) {
          if(!info.exported_terms.contains(request)) {
            std::cout << "No name " << request << " in module " << import_info.module_name << " requested from " << module_name << ".\n";
            return false;
          }
          if(!add_name(request, copy_on_arena(context.arena, info.exported_terms.at(request)))) return false;
        }
      }
    }
    auto compilation_result = map(
      resolve(std::move(parsed_module.second), {
        .names_to_values = names_to_values,
        .u64 = context.u64.get(),
        .str = context.str.get(),
        .empty_vec = new_expression::TypedValue{
          .value = context.arena.copy(context.empty_vec),
          .type = context.arena.copy(context.context.type_collector.get_type_of(context.empty_vec))
        },
        .cons_vec = new_expression::TypedValue{
          .value = context.arena.copy(context.cons_vec),
          .type = context.arena.copy(context.context.type_collector.get_type_of(context.cons_vec))
        }
      }),
      [&](auto resolved) {
        return evaluate(std::move(resolved), {
          .context = context.context
        });
      }
    );
    if(compilation_result.holds_error()) {
      std::cout << compilation_result.get_error() << "\n";
      return false;
    }
    auto& compilation = compilation_result.get_value();
    if(!compilation.is_okay()) {
      new_expression::WeakKeyMap<std::string> externals_to_names(context.arena);
      compilation.report_errors_to(std::cout, externals_to_names);
      return false;
    }
    new_expression::EvaluationContext ctx{context.arena, context.context.rule_collector};
    compilation.result.value = ctx.reduce(std::move(compilation.result.value));
    compilation.result.type = ctx.reduce(std::move(compilation.result.type));
    if(compilation.is_okay() && compilation.result.type == module_primitives.module_type) {
      std::unordered_map<std::string, new_expression::TypedValue> exported_terms;
      new_expression::WeakExpression module_head = unfold(context.arena, compilation.result.value).args[0];
      while(true) {
        auto unfolded = unfold(context.arena, module_head);
        if(unfolded.args.size() != 3) break;
        auto entry = unfolded.args[1]; //head;
        auto entry_unfolded = unfold(context.arena, entry);
        if(entry_unfolded.args.size() != 3) break; //???
        auto str = std::string{context.str->read_data(context.arena.get_data(entry_unfolded.args[1])).get_string()};
        auto type = entry_unfolded.args[0];
        auto value = entry_unfolded.args[2];
        module_head = unfolded.args[2];
        if(exported_terms.contains(str)) {
          std::cout << "Duplicated export " << str << " in module " << module_name << "\n";
          destroy_from_arena(context.arena, exported_terms, compilation);
          return false;
        }
        exported_terms.insert(std::make_pair(std::move(str), new_expression::TypedValue{
          .value = context.arena.copy(value),
          .type = context.arena.copy(type)
        }));
      }
      module_info.insert(std::make_pair(module_name, ModuleInfo{
        .exported_terms = std::move(exported_terms),
        .evaluate_info = std::move(compilation),
        .source = std::move(source)
      }));
      modules_loading.erase(module_name);
      module_loading_stack.pop_back();
      return true;
    } else {
      std::cout << "Module " << module_name << " did not return a module object.\n";
      destroy_from_arena(context.arena, compilation);
      return false;
    }
  }
};
bool ModuleLoadInfo::locate_and_prepare_module(std::string module_name, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives) {
  if(module_info.contains(module_name)) return true;
  std::string source;
  std::ifstream f("InnerLib/" + module_name);
  if(!f) {
    std::cout << "Failed to read file \"" << ("InnerLib/" + module_name) << "\"\n";
    return false;
  } else {
    std::getline(f, source, '\0'); //just read the whole file - assuming no null characters in it :P
  }
  return prepare_module(module_name, std::move(source), context, module_primitives);
}


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
      ModuleLoadInfo load_info;

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
        auto parsed_module_result = bind(
          pipeline::compile::lex({context.arena, line}),
          [&](auto lexed) {
            return parse_module(std::move(lexed));
          });
        if(parsed_module_result.holds_error()) {
          std::cout << parsed_module_result.get_error() << "\n";
          return false;
        }
        auto& parsed_module = parsed_module_result.get_value();
        std::unordered_map<std::string, new_expression::TypedValue> names_to_values;
        new_expression::RAIIDestroyer destroyer{arena, names_to_values};
        for(auto const& entry : context.names_to_values) {
          names_to_values.insert(std::make_pair(entry.first, copy_on_arena(context.arena, entry.second)));
        }
        auto add_name = [&](std::string name, new_expression::TypedValue value) {
          if(names_to_values.contains(name)) {
            destroy_from_arena(context.arena, value);
            std::cout << "Duplicated name " << name << " in imports of main expression.\n";
            return false;
          } else {
            names_to_values.insert(std::make_pair(std::move(name), std::move(value)));
            return true;
          }
        };
        for(auto const& import_info : parsed_module.first.imports) {
          if(!load_info.locate_and_prepare_module(import_info.module_name, context, module_info)) {
            std::terminate();
          }
          auto const& info = load_info.module_info.at(import_info.module_name);
          if(import_info.request_all) {
            for(auto const& entry : info.exported_terms) {
              if(!add_name(entry.first, copy_on_arena(context.arena, entry.second))) {
                std::terminate();
              }
            }
          } else {
            for(auto const& request : import_info.requested_names) {
              if(!info.exported_terms.contains(request)) {
                std::cout << "No name " << request << " in module " << import_info.module_name << " requested from  main.\n";
                std::terminate();
              }
              if(!add_name(request, copy_on_arena(context.arena, info.exported_terms.at(request)))) {
                std::terminate();
              }
            }
          }
        }
        auto compilation_result = map(
          resolve(std::move(parsed_module.second), {
            .names_to_values = names_to_values,
            .u64 = context.u64.get(),
            .str = context.str.get(),
            .empty_vec = new_expression::TypedValue{
              .value = context.arena.copy(context.empty_vec),
              .type = context.arena.copy(context.context.type_collector.get_type_of(context.empty_vec))
            },
            .cons_vec = new_expression::TypedValue{
              .value = context.arena.copy(context.cons_vec),
              .type = context.arena.copy(context.context.type_collector.get_type_of(context.cons_vec))
            }
          }),
          [&](auto resolved) {
            return evaluate(std::move(resolved), {
              .context = context.context
            });
          }
        );
        if(compilation_result.holds_error()) {
          std::cout << compilation_result.get_error() << "\n";
          return false;
        }

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

        if(auto* module_value = compilation_result.get_if_value()) {
          /*for(auto const& import : module_value->first.imports) {
            std::cout << "Import: " << import.module_name;
            if(import.request_all) std::cout << " (all)";
            for(auto const& request : import.requested_names) {
              std::cout << " " << request;
            }
            std::cout << "\n";
          }*/
          auto* value = module_value;//&module_value->second;
          value->report_errors_to(std::cout, {arena});
          auto namer = [&](std::ostream& o, new_expression::WeakExpression expr) {
            if(auto name = value->get_explicit_name_of(expr)) {
              o << *name;
              return;
            } else if(externals_to_names.contains(expr)) {
              o << externals_to_names.at(expr);
              return;
            } else {
              for(auto const& entry : load_info.module_info) {
                if(auto name = entry.second.evaluate_info.get_explicit_name_of(expr)) {
                  o << entry.first << "::" << *name;
                  return;
                }
              }
            }
            if(arena.holds_declaration(expr)) {
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
            std::vector<std::string> import_list;
            new_expression::WeakKeyMap<std::size_t> import_indices(arena);
            new_expression::WeakKeyMap<std::string> import_positions(arena);
            import_positions.set(context.context.primitives.type, "Type");
            import_positions.set(context.context.primitives.arrow, "arrow");
            import_positions.set(context.context.primitives.id, "id");
            import_positions.set(context.context.primitives.constant, "constant");
            import_positions.set(context.u64->type, "U64");
            import_positions.set(context.str->type, "String");
            import_positions.set(context.vec_type, "Vector");
            import_positions.set(context.empty_vec, "empty_vec");
            import_positions.set(context.cons_vec, "cons_vec");
            import_positions.set(module_info.module_type, "Module");
            import_positions.set(module_info.module_ctor, "module");
            import_positions.set(module_info.module_entry_type, "ModuleEntry");
            import_positions.set(module_info.module_entry_ctor, "module_entry");
            for(auto const& entry : load_info.module_info) {
              for(auto const& exported : entry.second.exported_terms) {
                import_positions.set(exported.second.value, entry.first + "::" + exported.first);
                import_positions.set(exported.second.type, entry.first + "::" + exported.first + ".type");
              }
            }
            auto core = expr_module::store({
              .arena = arena,
              .rule_collector = context.context.rule_collector,
              .type_collector = context.context.type_collector,
              .get_import_index_of = [&](new_expression::WeakExpression expr) -> std::optional<std::uint32_t> {
                if(import_indices.contains(expr)) {
                  return (std::uint32_t)import_indices.at(expr);
                } else if(import_positions.contains(expr)) {
                  auto ret = import_list.size();
                  import_list.push_back(import_positions.at(expr));
                  import_indices.set(expr, ret);
                  return (std::uint32_t)ret;
                } else {
                  return std::nullopt;
                }
              },
              .get_import_size = [&]() -> std::uint32_t { return (std::uint32_t)import_list.size(); }
            }, {
              .exports = std::move(exports)
            });
            std::cout << debug_format(core) << "\n";
            std::cout << "Imports:";
            for(auto const& i : import_list) {
              std::cout << " " << i;
            }
            std::cout << "\n";
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
          std::cout << compilation_result.get_error() << "\n";
        }
      }
      destroy_from_arena(arena, module_info, load_info);
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
