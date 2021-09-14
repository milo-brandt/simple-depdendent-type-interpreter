//#include "WebInterface/web_interface.hpp"
/*
#include "ExpressionParser/parser_tree.hpp"

#include "ExpressionParser/expression_parser.hpp"

#include <iostream>
#include "Utility/overloaded.hpp"
#include "Compiler/resolved_tree.hpp"
#include "Compiler/evaluator.hpp"
#include "Expression/solver.hpp"
#include "Expression/solver_context.hpp"
#include "Expression/formatter.hpp"
#include "Expression/rule_simplification.hpp"

#include <sstream>

std::optional<std::pair<expression::pattern::Tree, std::unordered_map<std::uint64_t, std::uint64_t> > > as_pattern(expression::tree::Tree const& tree, expression::Context& context) {
  struct Detail {
    expression::Context& context;
    std::unordered_map<std::uint64_t, std::uint64_t> map;
    std::optional<expression::pattern::Tree> evaluate(expression::tree::Tree const& tree, bool on_spine) {
      return tree.visit(mdb::overloaded{
        [&](expression::tree::Apply const& apply) -> std::optional<expression::pattern::Tree> {
          auto lhs = evaluate(apply.lhs, on_spine);
          if(!lhs) return std::nullopt;
          auto rhs = evaluate(apply.rhs, false);
          if(!rhs) return std::nullopt;
          return expression::pattern::Apply{std::move(*lhs), std::move(*rhs)};
        },
        [&](expression::tree::Arg const& arg) -> std::optional<expression::pattern::Tree> {
          if(map.contains(arg.index))
            return std::nullopt;
          map.insert(std::make_pair(arg.index, map.size()));
          return expression::pattern::Wildcard{};
        },
        [&](expression::tree::External const& ext) -> std::optional<expression::pattern::Tree> {
          if(on_spine || context.external_info.at(ext.index).is_axiom) {
            return expression::pattern::Fixed{ext.index};
          } else {
            return std::nullopt;
          }
        }
      });
    }
  };
  Detail detail{context};
  auto ret = detail.evaluate(tree, true);
  if(ret) {
    return std::make_pair(std::move(*ret), std::move(detail.map));
  } else {
    return std::nullopt;
  }
}
std::optional<expression::tree::Tree> as_replacement(expression::tree::Tree const& tree, std::unordered_map<std::uint64_t, std::uint64_t> const& map) {
  struct Detail {
    std::unordered_map<std::uint64_t, std::uint64_t> const& map;
    std::optional<expression::tree::Tree> evaluate(expression::tree::Tree const& tree) {
      return tree.visit(mdb::overloaded{
        [&](expression::tree::Apply const& apply) -> std::optional<expression::tree::Tree> {
          auto lhs = evaluate(apply.lhs);
          if(!lhs) return std::nullopt;
          auto rhs = evaluate(apply.rhs);
          if(!rhs) return std::nullopt;
          return expression::tree::Apply{std::move(*lhs), std::move(*rhs)};
        },
        [&](expression::tree::Arg const& arg) -> std::optional<expression::tree::Tree> {
          if(!map.contains(arg.index))
            return std::nullopt;
          return expression::tree::Arg{map.at(arg.index)};
        },
        [&](expression::tree::External const& ext) -> std::optional<expression::tree::Tree> {
          return ext;
        }
      });
    }
  };
  Detail detail{map};
  return detail.evaluate(tree);
}
std::optional<expression::Rule> create_rule(compiler::evaluate::Rule& rule, expression::Context& context) {
  rule.pattern = context.reduce(std::move(rule.pattern));
  rule.replacement = context.reduce(std::move(rule.replacement));
  if(auto pat = as_pattern(rule.pattern, context)) {
    if(auto rep = as_replacement(rule.replacement, pat->second)) {
      return expression::Rule{
        .pattern = std::move(pat->first),
        .replacement = std::move(*rep)
      };
    }
  }
  return std::nullopt;
}
bool try_to_insert_rule(compiler::evaluate::Rule& rule, expression::Context& context) {
  if(auto r = create_rule(rule, context)) {
    context.rules.push_back(std::move(*r));
    return true;
  } else {
    return false;
  }
}

struct NamedValue {
  std::string name;
};
using VariableReason = std::variant<
  NamedValue,
  compiler::evaluate::variable_explanation::Any,
  expression::solve::introduction_explanation::Any
>;

#include <sstream>

struct FormatState {
  std::uint64_t lambda_counter = 0;
  std::uint64_t hole_counter = 0;
  std::uint64_t hole_type_counter = 0;
  std::uint64_t lambda_codomain_counter = 0;
  std::uint64_t lambda_cast_body_counter = 0;
  std::uint64_t lambda_cast_domain_counter = 0;
  std::uint64_t apply_cast_lhs_counter = 0;
  std::uint64_t apply_cast_rhs_counter = 0;
  std::uint64_t apply_domain_counter = 0;
  std::uint64_t apply_codomain_counter = 0;
  std::uint64_t unknown_counter = 0;
  std::string get_name_for(compiler::evaluate::variable_explanation::Any const& explanation) {
    using namespace compiler::evaluate::variable_explanation;
    std::stringstream out;
    std::visit(mdb::overloaded{
      [&](ExplicitHoleValue const&) { out << "hole_" << (hole_counter++); },
      [&](ExplicitHoleType const&) { out << "hole_type_" << (hole_type_counter++); }, //todo: match counts between holes and holes type
      [&](LambdaDeclaration const&) { out << "lambda_" << (lambda_counter++); },
      [&](LambdaCodomain const&) { out << "lambda_codomain_" << (lambda_codomain_counter++); },
      [&](LambdaCastBody const&) { out << "lambda_body_" << (lambda_cast_body_counter++); },
      [&](LambdaCastDomain const&) { out << "lambda_domain_" << (lambda_cast_domain_counter++); },
      [&](ApplyCastLHS const&) { out << "apply_lhs_" << (apply_cast_lhs_counter++); },
      [&](ApplyCastRHS const&) { out << "apply_rhs_" << (apply_cast_rhs_counter++); },
      [&](ApplyDomain const&) { out << "apply_domain_" << (apply_domain_counter++); },
      [&](ApplyCodomain const&) { out << "apply_codomain_" << (apply_codomain_counter++); }
    }, explanation);
    return out.str();
  }
};
struct TruncatedPrinter {
  std::string_view string;
  std::uint64_t max_length = 20;
};
std::ostream& operator<<(std::ostream& out, TruncatedPrinter const& printer) {
  if(printer.string.size() > printer.max_length) {
    return out << printer.string.substr(0, printer.max_length - 3) << "...";
  } else {
    return out << printer.string;
  }
}
TruncatedPrinter truncated(std::string_view str, std::uint64_t max_length = 20) {
  return TruncatedPrinter{str, max_length};
}
struct ExplanationInfo {
  compiler::resolution::locator::Tree const& resolution_locator;
  expression_parser::locator::Tree const& parser_locator;
  std::string_view locate(compiler::resolution::path::Path const& path) {
    compiler::resolution::locator::Tree const* resolution_position = &resolution_locator;
    expression_parser::locator::Tree const* parse_position = &parser_locator;
    for(auto const& step : path.steps) {
      resolution_position = &path_step(*resolution_position, step);
      auto const* pos = resolution_position->visit([](auto const& v) -> decltype(auto) { return &v.relative_position; });
      if(auto* step = std::get_if<compiler::resolution::relative::Step>(pos)) {
        parse_position = &path_step(*parse_position, step->step);
      }
    }
    return parse_position->visit([](auto const& v) { return v.position; });
  }
};
struct FormatInstance {
  expression::Context& context;
  std::unordered_map<std::uint64_t, VariableReason> const& explanations;
  std::vector<expression::solve::SolveFragment> const* solve_parts;
  FormatState counters;
  std::unordered_map<std::uint64_t, std::string> existing_names;
  std::unordered_map<std::uint64_t, std::string> need_explanation;
  std::unordered_set<std::uint64_t> unknown_variables;
  std::string format_external(std::uint64_t x) {
    auto get_name = [&](auto&& getter) -> std::string {
      if(existing_names.contains(x)) {
        return existing_names.at(x);
      } else {
        std::string ret = getter();
        existing_names.insert(std::make_pair(x, ret));
        return ret;
      }
    };
    if(explanations.contains(x)) {
      auto const& explanation = explanations.at(x);
      if(auto named_value = std::get_if<NamedValue>(&explanation)) {
        return get_name([&] { return named_value->name; });
      } else if(auto evaluator_var = std::get_if<compiler::evaluate::variable_explanation::Any>(&explanation)) {
        auto name = get_name([&] { return counters.get_name_for(*evaluator_var); });
        need_explanation.insert(std::make_pair(x, name));
        return name;
      } else if(auto solver_var = std::get_if<expression::solve::introduction_explanation::Any>(&explanation)) {
        if(solve_parts) {
          auto name = [&] {
            auto const& exploded = std::get<expression::solve::introduction_explanation::Exploded>(*solver_var);
            std::stringstream out;
            auto const& asym_explosion = std::get<expression::solve::equation_disposition::AsymmetricExplode>((*solve_parts)[exploded.equation_index].disposition);
            out << format_external(asym_explosion.pattern_head) << "." << exploded.part_index;
            std::string name = out.str();
            return name;
          } ();
          return get_name([&] { return name; });
        }
      }
    }
    return get_name([&] {
      std::stringstream str;
      str << "_" << x;
      return str.str();
    });
  }
  std::string format(expression::tree::Tree const& tree, bool full_reduce = false) {
    return expression::format::format_expression(tree, context, {
      .format_external = [&](std::uint64_t x) { return format_external(x); },
      .arrow_external = context.primitives.arrow,
      .full_reduce = full_reduce
    }).result;
  }
  std::string format(expression::pattern::Tree const& tree, bool full_reduce = false) {
    return expression::format::format_pattern(tree, context, {
      .format_external = [&](std::uint64_t x) { return format_external(x); },
      .arrow_external = context.primitives.arrow,
      .full_reduce = full_reduce
    }).result;
  }
  std::string explain(ExplanationInfo info) {
    std::stringstream out;
    for(auto const& explain : need_explanation) {
      auto const& [var, name] = explain;
      auto const& reason = std::get<compiler::evaluate::variable_explanation::Any>(explanations.at(var));
      auto const& path = std::visit([](auto const& v) -> decltype(auto) { return v.source; }, reason);
      out << "\t" << name << " is from\n\t\t" << truncated(info.locate(path)) << "\n";
    }
    return out.str();
  }
  void clear_explanation() {
    need_explanation.clear();
    unknown_variables.clear();
  }
  void clear() {
    //counters = {};
    clear_explanation();
  }
};
*/


#include "ExpressionParser/expression_parser.hpp"
#include "Compiler/instructions.hpp"
#include "Compiler/evaluator.hpp"
#include "Expression/evaluation_context.hpp"
#include "Expression/expression_debug_format.hpp"
#include "Expression/solver.hpp"
#include "Expression/standard_solver_context.hpp"

#include <termcolor.hpp>
#include <algorithm>

template<class... Formats>
struct Format {
  std::string_view source;
  std::string_view part;
  std::tuple<Formats...> formats;
  Format(std::string_view source, std::string_view part, Formats... formats):source(source),part(part),formats(formats...) {}
};
template<class... Formats>
std::ostream& operator<<(std::ostream& o, Format<Formats...> const& format) {
  auto start_offset = format.part.begin() - format.source.begin();
  auto end_offset = format.part.end() - format.source.begin();
  o << format.source.substr(0, start_offset);
  std::apply([&](auto const&... formats){ (o << ... << formats); }, format.formats);
  return o << format.part << termcolor::reset << format.source.substr(end_offset);
}
template<class... Formats>
Format<Formats...> format_substring(std::string_view substring, std::string_view full_string, Formats... formats) {
  return Format<Formats...>{full_string, substring, std::move(formats)...};
}
auto format_error(std::string_view substring, std::string_view full) {
  return format_substring(substring, full, termcolor::red, termcolor::bold, termcolor::underline);
}

void debug_print_expr(expression::tree::Expression const& expr) {
  std::cout << expression::raw_format(expr) << "\n";
}
void debug_print_pattern(expression::pattern::Pattern const& pat) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(pat)) << "\n";
}
void debug_print_rule(expression::Rule const& rule) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
}

std::optional<expression::Rule> convert_to_rule(expression::tree::Expression const& pattern, expression::tree::Expression const& replacement, expression::Context const& context, std::unordered_set<std::uint64_t> const& indeterminates) {
  struct Detail {
    expression::Context const& context;
    std::unordered_set<std::uint64_t> const& indeterminates;
    std::unordered_map<std::uint64_t, std::uint64_t> arg_convert;
    std::optional<expression::pattern::Pattern> convert_to_pattern(expression::tree::Expression const& expr, bool spine) {
      return expr.visit(mdb::overloaded{
        [&](expression::tree::Apply const& apply) -> std::optional<expression::pattern::Pattern> {
          if(auto lhs = convert_to_pattern(apply.lhs, spine)) {
            if(auto rhs = convert_to_pattern(apply.rhs, false)) {
              return expression::pattern::Apply{
                .lhs = std::move(*lhs),
                .rhs = std::move(*rhs)
              };
            }
          }
          return std::nullopt;
        },
        [&](expression::tree::External const& external) -> std::optional<expression::pattern::Pattern> {
          if(context.external_info[external.external_index].is_axiom != spine && !indeterminates.contains(external.external_index)) {
            return expression::pattern::Fixed{external.external_index};
          } else {
            return std::nullopt;
          }
        },
        [&](expression::tree::Arg const& arg) -> std::optional<expression::pattern::Pattern> {
          if(arg_convert.contains(arg.arg_index)) return std::nullopt;
          auto index = arg_convert.size();
          arg_convert.insert(std::make_pair(arg.arg_index, index));
          return expression::pattern::Wildcard{};
        }
      });
    }
    std::optional<expression::tree::Expression> remap_replacement(expression::tree::Expression const& expr) {
      return expr.visit(mdb::overloaded{
        [&](expression::tree::Apply const& apply) -> std::optional<expression::tree::Expression> {
          if(auto lhs = remap_replacement(apply.lhs)) {
            if(auto rhs = remap_replacement(apply.rhs)) {
              return expression::tree::Apply{
                .lhs = std::move(*lhs),
                .rhs = std::move(*rhs)
              };
            }
          }
          return std::nullopt;
        },
        [&](expression::tree::External const& external) -> std::optional<expression::tree::Expression> {
          return external;
        },
        [&](expression::tree::Arg const& arg) -> std::optional<expression::tree::Expression> {
          if(arg_convert.contains(arg.arg_index)) {
            return expression::tree::Arg{arg_convert.at(arg.arg_index)};
          } else {
            return std::nullopt;
          }
        }
      });
    }
  };
  Detail detail{context, indeterminates};
  if(auto pat = detail.convert_to_pattern(pattern, true)) {
    if(auto rep = detail.remap_replacement(replacement)) {
      return expression::Rule{std::move(*pat), std::move(*rep)};
    }
  }
  return std::nullopt;
}
//block { axiom Nat : Type; axiom zero : Nat; declare f : Nat -> Nat; rule f zero = zero; f }
int main(int argc, char** argv) {
  /*located_output::Expression located_expr = located_output::Apply{
    .lhs = located_output::Identifier{.id = "hi", .position = "hi"},
    .rhs = located_output::Identifier{.id = "wassup", .position = "wassup"},
    .position = "hi wassup"
  };
  auto expr = located_expr.locator;
  auto x = archive(expr);
  std::cout << "Hello, werld!\n";
  x.root().visit(mdb::overloaded{
    [](locator::archive_part::Apply const& apply) {
      std::cout << "Apply!\n";
      std::cout << apply.index().index() << "\n";
    },
    [](auto const&) {
      std::cout << "o no\n";
    }
  });
  std::cout << format(expr) << "\n";
  std::cout << format(x.root()) << "\n";
  std::cout << format(x, [&](std::ostream& o, std::string_view x) { o << "\"" << x << "\""; }) << "\n";*/
  expression::Context expression_context;
  compiler::evaluate::EvaluateContext evaluator {
    .arrow_axiom = { .value = expression::tree::External{ expression_context.primitives.arrow }, .type = expression_context.primitives.arrow_type() },
    .type_axiom = { .value = expression::tree::External{ expression_context.primitives.type }, .type = expression::tree::External{ expression_context.primitives.type } },
    .type_family_over = [&] (expression::tree::Expression expr) {
      return expression::TypedValue{
        .value = expression::tree::Apply{
          .lhs = expression::tree::Apply{
            .lhs = expression::tree::External{ expression_context.primitives.arrow },
            .rhs = std::move(expr)
          },
          .rhs = expression::tree::External{ expression_context.primitives.type_constant_function }
        },
        .type = expression::tree::External{ expression_context.primitives.type }
      };
    },
    .embed = [&] (std::uint64_t index) -> expression::TypedValue {
      if(index == 0) {
        return { .value = expression::tree::External{ expression_context.primitives.type }, .type = expression::tree::External{ expression_context.primitives.type } };
      } else {
        return { .value = expression::tree::External{ expression_context.primitives.arrow}, .type = expression_context.primitives.arrow_type() };
      }
    },
    .allocate_variable = [&](bool is_axiom) -> std::uint64_t {
      if(is_axiom) {
        return expression_context.add_axiom();
      } else {
        return expression_context.add_declaration();
      }
    },
    .reduce = [&](expression::tree::Expression term) {
      return expression_context.reduce(std::move(term));
    }
  };
  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line == "q") return 0;
    std::string_view source = line;

    auto x = expression_parser::parser::expression(source);

    if(auto* error = parser::get_if_error(&x)) {
      std::cout << "Error: " << error->error << "\nAt: " << format_error(source.substr(error->position.begin() - source.begin()), source) << "\n";
    } else {
      auto& success = parser::get_success(x);
      auto output_archive = archive(success.value.output);
      auto locator_archive = archive(success.value.locator);
      std::cout << "Parse result: " << format(locator_archive) << "\n";
      auto resolved = expression_parser::resolve(expression_parser::resolved::ContextLambda{
        .lookup = [&](std::string_view str) -> std::optional<std::uint64_t> {
          if(str == "Type") {
            return 0;
          } else if(str == "arrow") {
            return 1;
          } else {
            return std::nullopt;
          }
        }
      }, output_archive.root());
      if(auto* resolve = resolved.get_if_value()) {
        std::cout << "Resolved: " << format(*resolve) << "\n";
        auto resolve_archive = archive(std::move(*resolve));
        auto instructions = compiler::instruction::make_instructions(resolve_archive.root());
        std::cout << "Instructions: " << format(instructions.output) << "\n";
        auto instruction_archive = archive(instructions.output);
        auto instruction_locator = archive(instructions.locator);
        auto eval_result = compiler::evaluate::evaluate_tree(instruction_archive.root().get_program_root(), evaluator);

        expression::solver::StandardSolverContext solver_context {
          .evaluation = expression_context
        };
        expression::solver::Solver solver(mdb::ref(solver_context));

        std::cout << "Variables:\n";
        for(auto const& [var, reason] : eval_result.variables) {
          std::cout << std::visit(mdb::overloaded{
            [&](compiler::evaluate::variable_explanation::ApplyRHSCast const&) { return "ApplyRHSCast: "; },
            [&](compiler::evaluate::variable_explanation::ApplyCodomain const&) { return "ApplyCodomain: "; },
            [&](compiler::evaluate::variable_explanation::ApplyLHSCast const&) { return "ApplyLHSCast: "; },
            [&](compiler::evaluate::variable_explanation::ExplicitHole const&) { return "ExplicitHole: "; },
            [&](compiler::evaluate::variable_explanation::Declaration const&) { return "Declaration: "; },
            [&](compiler::evaluate::variable_explanation::Axiom const&) { return "Axiom: "; },
            [&](compiler::evaluate::variable_explanation::TypeFamilyCast const&) { return "TypeFamilyCast: "; },
            [&](compiler::evaluate::variable_explanation::HoleTypeCast const&) { return "HoleTypeCast: "; },
            [&](compiler::evaluate::variable_explanation::DeclareTypeCast const&) { return "DeclareTypeCast: "; },
            [&](compiler::evaluate::variable_explanation::AxiomTypeCast const&) { return "AxiomTypeCast: "; },
            [&](compiler::evaluate::variable_explanation::LetTypeCast const&) { return "LetTypeCast: "; },
            [&](compiler::evaluate::variable_explanation::LetCast const&) { return "LetCast: "; },
            [&](compiler::evaluate::variable_explanation::ForAllTypeCast const&) { return "ForAllTypeCast: "; }
          }, reason) << var << "\n";
          if(is_variable(reason))
            solver.register_variable(var);
          if(is_indeterminate(reason))
            solver_context.indeterminates.insert(var);

          auto const& index = std::visit([&](auto const& reason) -> compiler::instruction::archive_index::PolymorphicKind {
            return reason.index;
          }, reason);

          auto const& pos = instruction_locator[index];
          auto const& locator_index = pos.visit([&](auto const& obj) { return obj.source.index; });
          auto const& locator_pos = locator_archive[locator_index];
          auto const& str_pos = locator_pos.visit([&](auto const& o) { return o.position; });
          std::cout << "Position: " << format_error(str_pos, source) << "\n";
        }
        std::cout << "\n";
        std::vector<std::pair<std::uint64_t, std::uint64_t> > casts;
        std::uint64_t cast_index = 0;
        for(auto const& cast : eval_result.casts) {
          solver_context.indeterminates.insert(cast.variable);
          std::cout << "Cast (depth " << cast.depth << ") " << expression::raw_format(cast.source) << " from " << expression::raw_format(cast.source_type) << " to " << expression::raw_format(cast.target_type) << " as " << cast.variable << "\n";
          auto index = solver.add_equation(cast.depth, cast.source_type, cast.target_type);
          casts.emplace_back(index, cast_index);
          ++cast_index;
        }
        std::vector<std::pair<std::uint64_t, std::uint64_t> > rules;
        std::uint64_t rule_index = 0;
        for(auto const& rule : eval_result.rules) {
          std::cout << "Rule (depth " << rule.depth << ") " << expression::raw_format(rule.pattern) << " from " << expression::raw_format(rule.pattern_type) << " to " << expression::raw_format(rule.replacement_type) << " as " << expression::raw_format(rule.replacement) << "\n";
          auto index = solver.add_equation(rule.depth, rule.pattern_type, rule.replacement_type);
          rules.emplace_back(index, rule_index);
          ++rule_index;
        }
        //Hard thing: How to know when rules are allowable?
        std::cout << "Result: " << expression::raw_format(eval_result.result.value) << " of type " << expression::raw_format(eval_result.result.type) << "\n";

        auto rule_count = expression_context.rules.size();

        while([&] {
          bool progress = false;
          progress |= solver.try_to_make_progress();
          auto cast_erase = std::remove_if(casts.begin(), casts.end(), [&](auto cast_info) {
            auto [equation_index, cast_index] = cast_info;
            if(solver.is_equation_satisfied(equation_index)) {
              auto const& cast = eval_result.casts[cast_index];
              solver_context.define_variable(cast.variable, cast.depth, cast.source);
              return true;
            } else {
              return false;
            }
          });
          if(cast_erase != casts.end()) {
            casts.erase(cast_erase, casts.end());
            progress = true;
          } //block { axiom One : Type; axiom one : One; declare id : One -> One; rule id one = one; id }
          auto rule_erase = std::remove_if(rules.begin(), rules.end(), [&](auto rule_info) {
            auto [equation_index, rule_index] = rule_info;
            if(solver.is_equation_satisfied(equation_index)) {
              auto& rule = eval_result.rules[rule_index];
              rule.pattern = expression_context.reduce(std::move(rule.pattern));
              rule.replacement = expression_context.reduce(std::move(rule.replacement));
              if(auto new_rule = convert_to_rule(rule.pattern, rule.replacement, expression_context, solver_context.indeterminates)) {
                expression_context.rules.push_back(std::move(*new_rule));
                return true;
              }
            }
            return false;
          }); //block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; declare double : Nat -> Nat; rule double zero = zero; rule double (succ n) = succ (succ (double n)); double }
          if(rule_erase != rules.end()) {
            rules.erase(rule_erase, rules.end());
            progress = true;
          }

          return progress;
        }());
        if(casts.empty()) {
          std::cout << "Casts okay!\n";
        }
        if(solver.is_fully_satisfied()) {
          std::cout << "Solver satisfied!\n";
        } else {
          for(auto const& [lhs, rhs] : solver.get_equations()) {
            std::cout << expression::raw_format(lhs) << " =?= " << expression::raw_format(rhs) << "\n";
          }
        }
        if(rules.empty()) {
          std::cout << "Rules okay!\n";
        } else {
          for(auto [equation_index, rule_index] : rules) {
            auto const& rule = eval_result.rules[rule_index];
            std::cout << "Rule failed (depth " << rule.depth << ") " << expression::raw_format(rule.pattern) << " from " << expression::raw_format(rule.pattern_type) << " to " << expression::raw_format(rule.replacement_type) << " as " << expression::raw_format(rule.replacement) << "\n";
          }
        }

        for(auto i = rule_count; i < expression_context.rules.size(); ++i) {
          auto const& rule = expression_context.rules[i];
          std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
        }
        std::cout << "Final: " << expression::raw_format(expression_context.reduce(eval_result.result.value)) << " of type " << expression::raw_format(expression_context.reduce(eval_result.result.type)) << "\n";
      } else {
        auto const& err = resolved.get_error();
        for(auto const& bad_id : err.bad_ids) {
          auto bad_pos = locator_archive[bad_id].position;
          std::cout << "Bad id: " << format_error(bad_pos, source) << "\n";
        }
        for(auto const& bad_id : err.bad_pattern_ids) {
          auto bad_pos = locator_archive[bad_id].position;
          std::cout << "Bad pattern id: " << format_error(bad_pos, source) << "\n";
        }
      }
    }
  }
/*
  expression::Context context;
  compiler::resolution::NameContext name_context;
  std::vector<expression::TypedValue> embeds;

  std::unordered_map<std::uint64_t, VariableReason> variable_explanations;

  FormatInstance formatter{
    .context = context,
    .explanations = variable_explanations
  };

  auto add_name = [&](std::string name, expression::TypedValue value) {
    name_context.names.insert_or_assign(std::move(name), compiler::resolution::NameInfo{
      .embed_index = embeds.size()
    });
    embeds.push_back(std::move(value));
  };
  auto add_named_external = [&](std::string name, std::uint64_t ext_index, expression::tree::Tree type) {
    variable_explanations.insert(std::make_pair(ext_index, NamedValue{name}));
    add_name(std::move(name), expression::TypedValue{
      .value = expression::tree::External{ext_index},
      .type = std::move(type)
    });
  };

  add_named_external("Type", context.primitives.type, expression::tree::External{context.primitives.type});
  add_named_external("arrow", context.primitives.arrow, context.primitives.arrow_type());
  auto nat_axiom = context.add_axiom();
  add_named_external("Nat", nat_axiom, expression::tree::External{context.primitives.type});
  add_named_external("zero", context.add_axiom(), expression::tree::External{nat_axiom});
  auto always_nat = context.add_declaration();
  context.rules.push_back({
    .pattern = expression::pattern::Apply{
      expression::pattern::Fixed{always_nat},
      expression::pattern::Wildcard{}
    },
    .replacement = expression::tree::External{nat_axiom}
  });
  add_named_external("succ", context.add_axiom(), expression::tree::Apply{
    expression::tree::Apply{
      expression::tree::External{context.primitives.arrow},
      expression::tree::External{nat_axiom}
    },
    expression::tree::External{always_nat}
  });



  std::size_t counter = 0;
  std::size_t rule_size = context.rules.size();
  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line == "q") return 0;

    std::string_view source = line;


    auto x = expression_parser::parser::expression(source);

    if(auto* error = parser::get_if_error(&x)) {
      std::cout << "Error: " << error->error << "\nAt char: " << (error->position.begin() - (std::string_view(source)).begin()) << "\n";
    } else {
      auto& success = parser::get_success(x);
      std::cout << "Parse result: ";
      format_indented(std::cout, success.value.locator, 0, [](auto& o, auto const& v) { o << v; });
      std::cout << "\n";

      auto resolve = compiler::resolution::resolve({
        .tree = success.value.output,
        .context = name_context,
        .embed_arrow = context.primitives.arrow
      });

      if(auto* err = resolve.get_if_error()) {
        std::cout << "Error!\n";
      } else {
        auto& resolved = resolve.get_value();
        std::cout << "Resolve result: ";
        format_indented(std::cout, resolved.tree.output, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";

        auto evaluation_result = compiler::evaluate::evaluate_tree(
          resolved.tree.output,
          {
            .arrow_axiom = context.primitives.arrow,
            .type_axiom = context.primitives.type,
            .embed = [&](std::uint64_t index) { return embeds.at(index); },
            .allocate_variable = [&]() { return context.add_declaration(); }
          }
        );

        expression::solve::SimpleContext solvey_context{context};
        expression::solve::Solver solvey{solvey_context};
        for(auto const& var : evaluation_result.variables) {
          if(!std::holds_alternative<compiler::evaluate::variable_explanation::LambdaDeclaration>(var.second)) {
            std::cout << "Variable registered: " << var.first << "\n";
            solvey.register_variable(var.first);
          } else {
            std::cout << "Declaration registered: " << var.first << "\n";
          }
        }
        std::vector<std::tuple<std::uint64_t, std::uint64_t, expression::tree::Tree, expression::tree::Tree> > casts;
        for(auto& cast : evaluation_result.casts) {
          std::cout << "Cast registered:\n";
          std::cout << "\tDepth: " << cast.depth << "\n";
          std::cout << "\tSource Type: " << expression::format::format_expression(cast.source_type, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tSource: " << expression::format::format_expression(cast.source, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tTarget Type: " << expression::format::format_expression(cast.target_type, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tTarget: " << expression::format::format_expression(cast.target, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          auto index = solvey.add_equation({
            .depth = cast.depth,
            .lhs = std::move(cast.source_type),
            .rhs = std::move(cast.target_type)
          });
          casts.emplace_back(index, cast.depth, std::move(cast.source), std::move(cast.target));
        }
        for(auto& rule : evaluation_result.rules) {
          std::cout << "Rule registered:\n";
          std::cout << "\tPattern: " << expression::format::format_expression(rule.pattern, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tReplacement: " << expression::format::format_expression(rule.replacement, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
        }
        for(auto const& var : evaluation_result.variables) {
          variable_explanations.insert(var);
        }
      SOLVE_HEAD:
        bool made_progress = solvey.deduce();
        auto casts_new_end = std::remove_if(casts.begin(), casts.end(), [&](auto& cast) {
          if(solvey.is_equation_satisfied(std::get<0>(cast))) {
            solvey.add_equation({
              .depth = std::get<1>(cast),
              .lhs = std::move(std::get<2>(cast)),
              .rhs = std::move(std::get<3>(cast))
            });
            return true;
          } else {
            return false;
          }
        });
        if(casts_new_end != casts.end()) {
          made_progress = true;
          casts.erase(casts_new_end, casts.end());
        }
        auto rules_new_end = std::remove_if(evaluation_result.rules.begin(), evaluation_result.rules.end(), [&](auto& rule) {
          return try_to_insert_rule(rule, context);
        });
        if(rules_new_end != evaluation_result.rules.end()) {
          made_progress = true;
          evaluation_result.rules.erase(rules_new_end, evaluation_result.rules.end());
        }
        if(made_progress) goto SOLVE_HEAD;
        for(auto const& var : solvey_context.introductions) {
          variable_explanations.insert(var);
        }
        formatter.solve_parts = &solvey.parts();
        if(!evaluation_result.rules.empty()) {
          std::cout << "Rules left unsatisfied!\n";
          for(auto const& rule : evaluation_result.rules) {
            std::cout
              << "Unsatisfied: " << expression::format::format_expression(rule.pattern, context, {
                .arrow_external = context.primitives.arrow
              }).result << " := " << expression::format::format_expression(rule.replacement, context, {
              .arrow_external = context.primitives.arrow
            }).result << "\n";
          }
        }
        if(solvey.is_fully_satisfied()) {
          std::cout << "Type solving succeeded.\n";
        } else {
          std::cout << "Type solving failed.\n";
          for(auto const& part : solvey.parts()) {
            if(std::visit(mdb::overloaded{
              [&](expression::solve::equation_disposition::Open const& open) -> bool {
                std::cout << "Open equation:\n";
                return true;
              },
              [&](expression::solve::equation_disposition::AxiomMismatch const& ax) -> bool {
                std::cout << "Axiom mismatch:\n";
                return true;
              },
              [&](expression::solve::equation_disposition::ArgumentMismatch const& ax) -> bool {
                std::cout << "Argument mismatch:\n";
                return true;
              },
              [&](auto const&) { return false; }
            }, part.disposition)) {
              std::cout
                << "\tLHS: " << formatter.format(part.lhs) << "\n"
                << "\tRHS: " << formatter.format(part.rhs) << "\n"
                << "where\n" << formatter.explain({
                  .resolution_locator = resolved.tree.locator,
                  .parser_locator = success.value.locator
                });
              formatter.clear_explanation();
            }
          }
        }
        {
          auto const& open_variables = solvey.variables();
          if(!open_variables.empty()) {
            bool first = true;
            std::cout << "Open variables: ";
            for(auto var : open_variables) {
              if(first) { first = false; } else { std::cout << ", "; }
              std::cout << var;
            }
            std::cout << "\n";
          }
        }
        expression::rule::DependencyFinder necessary_values{ context };

        auto ret_value = context.reduce(std::move(evaluation_result.result.value));
        auto ret_type = context.reduce(std::move(evaluation_result.result.type));

        for(std::size_t c = rule_size; c < context.rules.size(); ++c) {
          auto& rule = context.rules[c];
          rule = expression::rule::simplify_rule(rule, context);
        }

        necessary_values.examine_expression(ret_value);
        necessary_values.examine_expression(ret_type);

        auto needless_rules = std::remove_if(context.rules.begin() + rule_size, context.rules.end(), [&](auto const& rule) {
          return !necessary_values.seen.contains(expression::rule::get_rule_head(rule));
        });
        context.rules.erase(needless_rules, context.rules.end());

        for(std::size_t c = rule_size; c < context.rules.size(); ++c) {
          auto const& rule = context.rules[c];
          std::cout << "Pattern: " << formatter.format(rule.pattern) << " := " << formatter.format(rule.replacement) << "\n";
        }
        formatter.clear_explanation();
        rule_size = context.rules.size();


        std::cout << "Pretty print:\n" << formatter.format(ret_value, true) << "\n";
        std::cout << "Of type:\n" << formatter.format(ret_type, true) << "\n";

        std::cout << "where\n" << formatter.explain({
          .resolution_locator = resolved.tree.locator,
          .parser_locator = success.value.locator
        });
        formatter.clear();
        formatter.solve_parts = nullptr;

        std::stringstream str;
        str << "last" << counter++;
        add_name(str.str(), {std::move(ret_value), std::move(ret_type)});
      }
    }
  }*/
}

/*
block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; declare add : Nat -> Nat -> Nat; rule add zero x = x; rule add (succ x) y = succ (add x y); declare mul : Nat -> Nat -> Nat; rule mul zero x = zero; rule mul (succ x) y = add y (mul x y); mul }
*/

/*
block { axiom String : Type; axiom One : Type; axiom Parser : Type -> Type; axiom Recognize : (T: Type) -> String -> Parser T -> Parser T; axiom Accept : (T : Type) -> T -> Parser T; axiom dot : String; f Recognize _ dot \\ Recognize _ dot \\ Accept _ Type }
*/
/*
block { axiom Id : Type -> Type; axiom pure : (T : Type) -> T -> Id T; declare extract : (T : Type) -> Id T -> T; rule extract T (pure T x) = x; extract }
block { axiom Id : Type -> Type; axiom pure : (T : Type) -> T -> Id T; pure }

*/
