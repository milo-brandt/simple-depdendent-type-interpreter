#include "ExpressionParser/expression_parser.hpp"
#include "Compiler/instructions.hpp"
#include "Compiler/evaluator.hpp"
#include "Expression/evaluation_context.hpp"
#include "Expression/expression_debug_format.hpp"
#include "Expression/solver.hpp"
#include "Expression/standard_solver_context.hpp"

#include <termcolor.hpp>
#include <algorithm>
#include <map>

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
  auto end_length = format.source.end() - format.part.end();
  if(start_offset > 30) {
    o << "..." << format.source.substr(start_offset - 30, 30);
  } else {
    o << format.source.substr(0, start_offset);
  }
  std::apply([&](auto const&... formats){ (o << ... << formats); }, format.formats);
  o << format.part << termcolor::reset;
  if(end_length > 30) {
    o << format.source.substr(end_offset, 30) << "...";
  } else {
    o << format.source.substr(end_offset);
  }
  return o;
}
template<class... Formats>
Format<Formats...> format_substring(std::string_view substring, std::string_view full_string, Formats... formats) {
  return Format<Formats...>{full_string, substring, std::move(formats)...};
}
auto format_error(std::string_view substring, std::string_view full) {
  return format_substring(substring, full, termcolor::red, termcolor::bold, termcolor::underline);
}
auto format_info(std::string_view substring, std::string_view full) {
  return format_substring(substring, full, termcolor::green, termcolor::bold);
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
  auto embed = [&] (std::uint64_t index) -> expression::TypedValue {
    if(index == 0) {
      return expression_context.get_external(0);
    } else if(index == 1) {
      return expression_context.get_external(1);
    } else if(index == 2) {
      return expression_context.get_external(2);
    } else if(index == 3) {
      return expression_context.get_external(3);
    } else if(index == 4) {
      return expression_context.get_external(8);
    } else {
      std::terminate();
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
          } else if(str == "type_family") {
            return 2;
          } else if(str == "constant") {
            return 3;
          } else if(str == "id") {
            return 4;
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

        auto rule_count = expression_context.rules.size();

        auto eval_result = compiler::evaluate::evaluate_tree(instruction_archive.root().get_program_root(), expression_context, embed);

        expression::solver::StandardSolverContext solver_context {
          .evaluation = expression_context
        };
        expression::solver::Solver solver(mdb::ref(solver_context));

        std::cout << "Variables:\n";
        std::map<std::uint64_t, compiler::evaluate::variable_explanation::Any> sorted_variables;
        for(auto const& entry : eval_result.variables) sorted_variables.insert(entry);
        for(auto const& [var, reason] : sorted_variables) {
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
            [&](compiler::evaluate::variable_explanation::ForAllTypeCast const&) { return "ForAllTypeCast: "; },
            [&](compiler::evaluate::variable_explanation::VarType const&) { return "VarType: "; }
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
          std::cout << "Position: " << format_info(str_pos, source) << "\n";
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
          }
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
          });
          if(rule_erase != rules.end()) {
            rules.erase(rule_erase, rules.end());
            progress = true;
          }

          return progress;
        }());
        for(auto i = rule_count; i < expression_context.rules.size(); ++i) {
          auto const& rule = expression_context.rules[i];
          std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
        }
        rule_count = expression_context.rules.size();
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
            if(auto constrained_pat = compiler::pattern::from_expression(rule.pattern, expression_context, solver_context.indeterminates)) {
              std::cout << "Resolved fancy rule:\n";
              std::cout << format(constrained_pat->pattern) << "\n";
              for(auto const& constraint : constrained_pat->constraints) {
                std::cout << "Capture point " << constraint.capture_point << " =?= " << expression::raw_format(constraint.equivalent_expression) << "\n";
              }

              auto archived = archive(constrained_pat->pattern);
              auto evaluated = compiler::evaluate::evaluate_pattern(archived.root(), expression_context);
              std::vector<std::pair<std::uint64_t, std::uint64_t> > pat_casts;
              expression::solver::Solver pat_solver(mdb::ref(solver_context));

              std::uint64_t pat_cast_index = 0;
              for(auto const& cast : evaluated.casts) {
                std::cout << "Cast (depth " << cast.depth << ") " << expression::raw_format(cast.source) << " from " << expression::raw_format(cast.source_type) << " to " << expression::raw_format(cast.target_type) << " as " << cast.variable << "\n";
                auto index = pat_solver.add_equation(cast.depth, cast.source_type, cast.target_type);
                pat_casts.emplace_back(index, pat_cast_index);
                ++pat_cast_index;
              }
              for(auto const& [var, reason] : evaluated.variables) {
                solver_context.indeterminates.insert(var);
                if(!std::holds_alternative<compiler::evaluate::pattern_variable_explanation::ApplyCast>(reason)) {
                  pat_solver.register_variable(var);
                  std::cout << "Cast: " << var << "\n";
                } else {
                  std::cout << "Variable: " << var << "\n";
                }
              }
              std::cout << "Capture points:";
              for(auto cast_var : evaluated.capture_point_variables) {
                std::cout << " " << cast_var;
              }
              std::cout << "\n";
              while([&] {
                bool progress = false;
                progress |= pat_solver.try_to_make_progress();
                auto cast_erase = std::remove_if(pat_casts.begin(), pat_casts.end(), [&](auto cast_info) {
                  auto [equation_index, cast_index] = cast_info;
                  if(pat_solver.is_equation_satisfied(equation_index)) {
                    auto const& cast = evaluated.casts[cast_index];
                    solver_context.define_variable(cast.variable, cast.depth, cast.source);
                    return true;
                  } else {
                    return false;
                  }
                });
                if(cast_erase != pat_casts.end()) {
                  pat_casts.erase(cast_erase, pat_casts.end());
                  progress = true;
                }
                return progress;
              }());
              for(auto i = rule_count; i < expression_context.rules.size(); ++i) {
                auto const& rule = expression_context.rules[i];
                std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
              }
              rule_count = expression_context.rules.size();
              if(pat_casts.empty()) {
                std::cout << "Pat casts okay!\n";
              }
              if(pat_solver.is_fully_satisfied()) {
                std::cout << "Pat solver satisfied!\n";
              } else {
                for(auto const& [lhs, rhs] : pat_solver.get_equations()) {
                  std::cout << expression::raw_format(lhs) << " =?= " << expression::raw_format(rhs) << "\n";
                }
              }

              std::vector<expression::tree::Expression> capture_vec;
              for(auto cast_var : evaluated.capture_point_variables) capture_vec.push_back(expression::tree::External{cast_var});
              for(auto constraint : constrained_pat->constraints) {
                auto base_value = capture_vec[constraint.capture_point];
                auto new_value = expression::substitute_into_replacement(capture_vec, constraint.equivalent_expression);
                std::cout << "Check: " << expression::raw_format(base_value) << " =?= " << expression::raw_format(new_value);
                auto base_simp = expression_context.reduce(std::move(base_value));
                auto new_simp = expression_context.reduce(std::move(new_value));
                if(base_simp == new_simp) std::cout << " (OKAY)\n";
                else {
                  std::cout << "\nFailed: " << expression::raw_format(base_simp) << " =?= " << expression::raw_format(new_simp) << "\n";
                }
              }
            }
          }
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
}

/*
block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; declare add : Nat -> Nat -> Nat; rule add zero x = x; rule add (succ x) y = succ (add x y); declare mul : Nat -> Nat -> Nat; rule mul zero x = zero; rule mul (succ x) y = add y (mul x y); mul (succ \\ succ \\ succ \\ succ zero) (succ \\ succ \\ succ \\ succ \\ succ zero) }
*/

/*
block { axiom String : Type; axiom One : Type; axiom Parser : Type -> Type; axiom Recognize : (T: Type) -> String -> Parser T -> Parser T; axiom Accept : (T : Type) -> T -> Parser T; axiom dot : String; f Recognize _ dot \\ Recognize _ dot \\ Accept _ Type }
*/
/*
block { axiom Id : Type -> Type; axiom pure : (T : Type) -> T -> Id T; declare extract : (T : Type) -> Id T -> T; rule extract T (pure T x) = x; extract }
block { axiom Id : Type -> Type; axiom pure : (T : Type) -> T -> Id T; pure }

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Vec : Nat -> Type; axiom empty : Vec zero; axiom cons : (n : Nat) -> Nat -> Vec n -> Vec (succ n); declare fold : Nat -> (Nat -> Nat -> Nat) -> (n : Nat) -> Vec n -> Nat; fold }

block { axiom Nat : Type; declare mul : Nat -> Nat -> Nat; axiom Square : Nat -> Type; axiom witness : (n : Nat) -> Square (mul n n); declare sqrt : (n : Nat) -> (Square n) -> Nat; rule sqrt (mul n n) (witness n) = n; sqrt }

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Vec : Nat -> Type; axiom empty : Vec zero; axiom cons : (n : Nat) -> Nat -> Vec n -> Vec (succ n); declare fold : Nat -> (Nat -> Nat -> Nat) -> (n : Nat) -> Vec n -> Nat; rule fold base combine zero empty = base; rule fold base combine (succ n) (cons n head tail) = fold (combine base head) combine n tail; fold }
block {
  axiom Nat : Type;
  axiom zero : Nat;
  axiom succ : Nat -> Nat;
  axiom Routine : Type -> Type;
  axiom return : (T : Type) -> T -> Routine T;
  axiom read : (T : Type) -> (Nat -> Routine T) -> Routine T;

  declare bind : (S : Type) -> (T : Type) -> Routine S -> (S -> Routine T) -> Routine T;
  bind S T (return S x) then = then x;
  bind S T (read S f) then = read T \n.bind S T (f n) then;
  bind
}


...this could be a good use case for coroutines, probably? - let things wait for whatever's blocking their progress
*/
