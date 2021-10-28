#include <fstream>
#include "User/interactive_environment.hpp"
#include "User/debug_format.hpp"
#include "Utility/vector_utility.hpp"
#include "Module/execute.hpp"
#include "Module/core_debug_print.hpp"
#include "Module/store.hpp"


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

expr_module::Core get_other() {
  using namespace expr_module::step;
  using namespace expr_module::rule_step;
  using namespace expr_module::rule_replacement;
  return {
  .value_import_size = 4,
  .data_type_import_size = 0,
  .c_replacement_import_size = 0,
  .register_count = 20,
  .output_size = 1,
  .steps = {
    Declare{0},
    Declare{1},
    Declare{2},
    Declare{3},
    Declare{4},
    Declare{5},
    Declare{6},
    Declare{7},
    Declare{8},
    Declare{9},
    Declare{10},
    Declare{11},
    Declare{12},
    Declare{13},
    Declare{14},
    Declare{15},
    Embed{0, 16},
    RegisterType{15, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 13, 16},
    RegisterType{14, 16},
    Embed{0, 16},
    RegisterType{13, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 13, 16},
    RegisterType{0, 16},
    Embed{0, 16},
    RegisterType{7, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 13, 16},
    Apply{12, 16, 16},
    RegisterType{8, 16},
    Apply{8, 0, 16},
    RegisterType{1, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 7, 16},
    RegisterType{5, 16},
    Embed{1, 16},
    Embed{3, 17},
    Embed{0, 18},
    Apply{17, 18, 17},
    Apply{17, 13, 17},
    Apply{16, 17, 16},
    Embed{2, 17},
    Embed{0, 18},
    Apply{17, 18, 17},
    Embed{0, 18},
    Apply{17, 18, 17},
    Embed{3, 18},
    Embed{0, 19},
    Apply{18, 19, 18},
    Apply{18, 13, 18},
    Apply{17, 18, 17},
    Apply{16, 17, 16},
    RegisterType{6, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 13, 16},
    RegisterType{2, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 4, 16},
    RegisterType{3, 16},
    Embed{0, 16},
    RegisterType{4, 16},
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Apply{16, 11, 16},
    RegisterType{9, 16},
    Apply{8, 2, 16},
    RegisterType{10, 16},
    Embed{0, 16},
    RegisterType{11, 16},
    Embed{0, 16},
    Apply{12, 16, 16},
    RegisterType{12, 16},
    Rule{
      .head = 15,
      .args_captured = 0,
      .steps = {
      },
      .replacement = Substitution{5}
    },
    Embed{0, 16},
    Rule{
      .head = 14,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Embed{1, 16},
    Apply{16, 5, 16},
    Apply{16, 14, 16},
    Rule{
      .head = 13,
      .args_captured = 0,
      .steps = {
      },
      .replacement = Substitution{16}
    },
    Embed{0, 16},
    Rule{
      .head = 0,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Embed{0, 16},
    Rule{
      .head = 7,
      .args_captured = 0,
      .steps = {
      },
      .replacement = Substitution{16}
    },
    Embed{3, 16},
    Embed{0, 17},
    Apply{16, 17, 16},
    Argument{0, 17},
    Apply{6, 17, 17},
    Apply{16, 17, 16},
    Rule{
      .head = 8,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Argument{0, 16},
    Rule{
      .head = 1,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Embed{0, 16},
    Rule{
      .head = 5,
      .args_captured = 0,
      .steps = {
      },
      .replacement = Substitution{16}
    },
    Embed{1, 16},
    Apply{16, 15, 16},
    Argument{0, 17},
    Apply{16, 17, 16},
    Rule{
      .head = 6,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Argument{0, 16},
    Apply{1, 16, 16},
    Apply{12, 16, 16},
    Rule{
      .head = 2,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Embed{1, 16},
    Argument{0, 17},
    Apply{16, 17, 16},
    Argument{0, 17},
    Apply{10, 17, 17},
    Apply{16, 17, 16},
    Rule{
      .head = 3,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Embed{1, 16},
    Apply{16, 5, 16},
    Apply{16, 9, 16},
    Rule{
      .head = 4,
      .args_captured = 0,
      .steps = {
      },
      .replacement = Substitution{16}
    },
    Embed{0, 16},
    Rule{
      .head = 9,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Argument{0, 16},
    Rule{
      .head = 10,
      .args_captured = 2,
      .steps = {
        PullArgument{},
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Apply{12, 5, 16},
    Rule{
      .head = 11,
      .args_captured = 0,
      .steps = {
      },
      .replacement = Substitution{16}
    },
    Embed{1, 16},
    Argument{0, 17},
    Apply{16, 17, 16},
    Embed{2, 17},
    Embed{0, 18},
    Apply{17, 18, 17},
    Embed{0, 18},
    Apply{17, 18, 17},
    Argument{0, 18},
    Apply{17, 18, 17},
    Apply{16, 17, 16},
    Rule{
      .head = 12,
      .args_captured = 1,
      .steps = {
        PullArgument{}
      },
      .replacement = Substitution{16}
    },
    Export{3}
  }
};
}
expr_module::Core get_prelude() {
  using namespace expr_module::step;
  using namespace expr_module::rule_step;
  using namespace expr_module::rule_replacement;
  return expr_module::Core{
    .value_import_size = 5, //Type, arrow, constant, U64, u64
    .data_type_import_size = 1, //u64
    .c_replacement_import_size = 1, //add
    .register_count = 7,
    .output_size = 1,
    .steps = {
      //First: Create a the value "U64 -> U64 -> U64"
      //= arrow U64 (constant Type (arrow U64 (constant Type U64 U64)) U64)
      Embed{0, 0},
      Embed{1, 1},
      Embed{2, 2},
      Embed{3, 3},
      Apply{1, 3, 4}, //arrow U64
      Apply{2, 0, 5}, //constant Type
      Apply{5, 3, 6}, //constant Type U64
      Apply{6, 3, 6}, //constant Type U64 U64
      Apply{4, 6, 6}, //arrow U64 (constant Type U64 U64)
      Apply{5, 6, 6}, //constant Type (arrow U64 (constant Type U64 U64))
      Apply{6, 3, 6}, //constant Type (arrow U64 (constant Type U64 U64)) U64
      Apply{4, 6, 6}, //arrow U64 (constant Type (arrow U64 (constant Type U64 U64)) U64)
      Declare{0}, //add
      RegisterType{0, 6},
      Export{0},
      Embed{4, 1},
      Argument{0, 2},
      Argument{2, 3},
      Rule{
        .head = 0,
        .args_captured = 2,
        .steps = {
          PullArgument{},
          PatternMatch{
            .substitution = 2,
            .expected_head = 1,
            .args_captured = 1
          },
          DataCheck{
            .capture_index = 1,
            .data_type_index = 0
          },
          PullArgument{},
          PatternMatch{
            .substitution = 3,
            .expected_head = 1,
            .args_captured = 1
          },
          DataCheck{
            .capture_index = 3,
            .data_type_index = 0
          }
        },
        .replacement = Function{
          .function_index = 0
        }
      }
    }
  };
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
  std::cout << debug_format(get_prelude()) << "\n";
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
      interactive::Environment environment(arena);
      auto& context = environment.context();
      auto add_replacement = [&](std::span<new_expression::WeakExpression> inputs) {
        return arena.apply(
          arena.copy(environment.u64_head()),
          environment.u64()->make_expression(
            environment.u64()->read_data(arena.get_data(inputs[1])) + environment.u64()->read_data(arena.get_data(inputs[3]))
          )
        );
      };
      auto ret = expr_module::execute({
        .arena = arena,
        .rule_collector = environment.context().rule_collector,
        .type_collector = environment.context().type_collector
      }, get_prelude(), {
        .value_imports = mdb::make_vector(
          arena.copy(environment.context().primitives.type),
          arena.copy(environment.context().primitives.arrow),
          arena.copy(environment.context().primitives.constant),
          arena.copy(environment.u64_type()),
          arena.copy(environment.u64_head())
        ),
        .data_type_import_indices = {
          environment.u64()->type_index
        },
        .c_replacement_imports = mdb::make_vector<mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)> >(
          add_replacement
        )
      });
      environment.name_primitive("add", ret.exports[0]);
      destroy_from_arena(arena, ret);

      ret = expr_module::execute({
        .arena = arena,
        .rule_collector = environment.context().rule_collector,
        .type_collector = environment.context().type_collector
      }, get_other(), {
        .value_imports = mdb::make_vector(
          arena.copy(environment.context().primitives.type),
          arena.copy(environment.context().primitives.arrow),
          arena.copy(environment.context().primitives.constant),
          arena.copy(environment.context().primitives.id)
        )
      });
      environment.name_primitive("Endo", ret.exports[0]);
      destroy_from_arena(arena, ret);

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
      auto sub_u64 = environment.declare("sub", "U64 -> U64 -> U64");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(sub_u64),
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
                environment.u64()->read_data(arena.get_data(inputs[1])) - environment.u64()->read_data(arena.get_data(inputs[3]))
              )
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
                exponent(environment.u64()->read_data(arena.get_data(inputs[1])), environment.u64()->read_data(arena.get_data(inputs[3])))
              )
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
              new_expression::PatternMatch{
                .substitution = arena.argument(0),
                .expected_head = arena.copy(environment.u64_head()),
                .args_captured = 1
              },
              new_expression::DataCheck{
                .capture_index = 1,
                .expected_type = environment.str()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return arena.apply(
              arena.copy(environment.u64_head()),
              environment.u64()->make_expression(
                environment.str()->read_data(arena.get_data(inputs[1])).size()
              )
            );
          }
        }
      });
      auto substr = environment.declare("substr", "String -> U64 -> U64 -> String");
      context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(substr),
          .body = {
            .args_captured = 3,
            .steps = mdb::make_vector<new_expression::PatternStep>(
              new_expression::PullArgument{},
              new_expression::PatternMatch{
                .substitution = arena.argument(0),
                .expected_head = arena.copy(environment.str_head()),
                .args_captured = 1
              },
              new_expression::DataCheck{
                .capture_index = 1,
                .expected_type = environment.str()->type_index
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
              },
              new_expression::PullArgument{},
              new_expression::PatternMatch{
                .substitution = arena.argument(4),
                .expected_head = arena.copy(environment.u64_head()),
                .args_captured = 1
              },
              new_expression::DataCheck{
                .capture_index = 5,
                .expected_type = environment.u64()->type_index
              }
            )
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [&](std::span<new_expression::WeakExpression> inputs) {
            return arena.apply(
              arena.copy(environment.str_head()),
              environment.str()->make_expression(
                environment.str()->read_data(arena.get_data(inputs[1])).substr(
                  environment.u64()->read_data(arena.get_data(inputs[3])),
                  environment.u64()->read_data(arena.get_data(inputs[5]))
                )
              )
            );
          }
        }
      });

      {
        auto result = environment.full_compile(source);
        if(auto* value = result.get_if_value()) {
          value->report_errors_to(std::cout, environment.externals_to_names());
          auto namer = [&](std::ostream& o, new_expression::WeakExpression expr) {
            if(auto name = value->get_explicit_name_of(expr)) {
              o << *name;
            } else if(environment.externals_to_names().contains(expr)) {
              o << environment.externals_to_names().at(expr);
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
          new_expression::EvaluationContext ctx{arena, context.rule_collector};
          value->result.value = ctx.reduce(std::move(value->result.value));
          value->result.type = ctx.reduce(std::move(value->result.type));
          std::cout << user::raw_format(arena, value->result.value, namer) << "\n";
          std::cout << user::raw_format(arena, value->result.type, namer) << "\n";
          auto exports = mdb::make_vector<new_expression::OwnedExpression>(
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
          })) << "\n";

          destroy_from_arena(arena, *value);


        } else {
          std::cout << result.get_error() << "\n";
        }
      }
      destroy_from_arena(arena, mul_u64, sub_u64, exp_u64, len, substr);
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
