#include <fstream>
#include "Top/interactive_environment.hpp"
#include "Top/debug_format.hpp"
#include "Top/module_loader.hpp"
#include "Utility/vector_utility.hpp"
#include "Module/execute.hpp"
#include "Module/core_debug_print.hpp"
#include "Module/store.hpp"
#include "NewExpression/arena_utility.hpp"
#include "Pipeline/standard_compiler_context.hpp"
#include "Pipeline/compile_stages.hpp"
#include "Top/run_source.hpp"
#include <dlfcn.h>

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
      pipeline::compile::StandardCompilerContext context{arena};
      auto result = run_source(arena, context, source);
      if(auto* value = result.get_if_value()) {
        auto& [compilation_result, load_info] = *value;

        new_expression::WeakKeyMap<std::string> externals_to_names(arena);
        externals_to_names.set(context.context.primitives.type, "Type");
        externals_to_names.set(context.context.primitives.arrow, "arrow");
        externals_to_names.set(context.u64->type, "U64");
        externals_to_names.set(context.str->type, "String");
        externals_to_names.set(context.vec_type, "Vector");
        externals_to_names.set(context.empty_vec, "empty_vec");
        externals_to_names.set(context.cons_vec, "cons_vec");
        //externals_to_names.set(module_info.module_type, "Module");
        //externals_to_names.set(module_info.full_module_ctor, "full_module");
        //externals_to_names.set(module_info.module_entry_type, "ModuleEntry");
        //externals_to_names.set(module_info.module_entry_ctor, "module_entry");

        if(auto* module_value = &compilation_result) {
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
          /*if(value->is_okay() && value->result.type == module_info.module_type) {
            std::cout << "Module!\n";
            new_expression::WeakExpression module_head = unfold(arena, value->result.value).args[1];
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
            import_positions.set(module_info.full_module_ctor, "module");
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
          }*/
          load_info.close();
          destroy_from_arena(arena, compilation_result, load_info);
        }
      }
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
