#include "ExpressionParser/expression_parser.hpp"
#include "Compiler/instructions.hpp"
#include "Compiler/evaluator.hpp"
#include "Expression/evaluation_context.hpp"
#include "Expression/expression_debug_format.hpp"
#include "Expression/solver.hpp"
#include "Expression/standard_solver_context.hpp"
#include "Expression/solve_routine.hpp"
#include "Expression/formatter.hpp"

#include <termcolor.hpp>
#include <algorithm>
#include <map>
#include <fstream>

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

  auto line_start = 0;
  auto line_count = 0;
  if(start_offset > 0) {
    auto last_line = format.source.rfind('\n', start_offset - 1);
    line_count = std::count(format.source.begin(), format.source.begin() + start_offset, '\n');
    if(last_line != std::string::npos) {
      line_start = last_line + 1;
    }
  }
  auto line_end = format.source.size();
  {
    auto next_line = format.source.find('\n', end_offset);
    if(next_line != std::string::npos) {
      line_end = next_line;
    }
  }

  if(start_offset - line_start > 30) {
    o << "..." << format.source.substr(start_offset - 30, 30);
  } else {
    o << format.source.substr(line_start, start_offset - line_start);
  }
  std::apply([&](auto const&... formats){ (o << ... << formats); }, format.formats);
  o << format.part << termcolor::reset;
  if(line_end - end_offset  > 30) {
    o << format.source.substr(end_offset, 30) << "...";
  } else {
    o << format.source.substr(end_offset, line_end - end_offset);
  }
  if(line_count > 0) {
    o << " [line " << (line_count + 1) << "]";
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
    if(line.empty()) continue;
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
        expression::solver::Routine solve_routine{eval_result, expression_context, solver_context};

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
          auto const& index = std::visit([&](auto const& reason) -> compiler::instruction::archive_index::PolymorphicKind {
            return reason.index;
          }, reason);
          auto const& pos = instruction_locator[index];
          auto const& locator_index = pos.visit([&](auto const& obj) { return obj.source.index; });
          auto const& locator_pos = locator_archive[locator_index];
          auto const& str_pos = locator_pos.visit([&](auto const& o) { return o.position; });
          std::cout << "Position: " << format_info(str_pos, source) << "\n";
        }
        for(auto const& cast : eval_result.casts) {
          std::cout << "Cast (depth " << cast.depth << ") " << expression::raw_format(cast.source) << " from " << expression::raw_format(cast.source_type) << " to " << expression::raw_format(cast.target_type) << " as " << cast.variable << "\n";
        }
        for(auto const& rule : eval_result.rules) {
          std::cout << "Rule (depth " << rule.depth << ") " << expression::raw_format(rule.pattern) << " from " << expression::raw_format(rule.pattern_type) << " to " << expression::raw_format(rule.replacement_type) << " as " << expression::raw_format(rule.replacement) << "\n";
        }
        //Hard thing: How to know when rules are allowable?
        std::cout << "Result: " << expression::raw_format(eval_result.result.value) << " of type " << expression::raw_format(eval_result.result.type) << "\n";

        solve_routine.run();

        std::vector<expression::Rule> new_rules;
        for(auto i = rule_count; i < expression_context.rules.size(); ++i) {
          new_rules.push_back(expression_context.rules[i]);
        }
        std::sort(new_rules.begin(), new_rules.end(), [](auto const& lhs, auto const& rhs) {
          struct Detail {
            static std::uint64_t get_head(expression::pattern::Pattern const& pat) {
              if(auto* apply = pat.get_if_apply()) {
                return get_head(apply->lhs);
              } else {
                return pat.get_fixed().external_index;
              }
            }
          };
          return Detail::get_head(lhs.pattern) < Detail::get_head(rhs.pattern);
        });
        for(auto const& rule : new_rules) {
          std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
        }
        rule_count = expression_context.rules.size();
        /*if(casts.empty()) {
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
          */
        std::unordered_map<std::uint64_t, const char*> fixed_names = {
          {0, "Type"},
          {1, "arrow"}
        };
        auto fancy_format = expression::format::FormatContext{
          .expression_context = expression_context,
          .force_expansion = [](std::uint64_t){ return true; },
          .write_external = [&](std::ostream& o, std::uint64_t ext_index) -> std::ostream& {
            if(fixed_names.contains(ext_index)) {
              return o << fixed_names.at(ext_index);
            } else if(eval_result.variables.contains(ext_index)) {
              namespace explanation = compiler::evaluate::variable_explanation;
              auto const& reason = eval_result.variables.at(ext_index);
              if(auto* declared = std::get_if<explanation::Declaration>(&reason)) {
                auto const& location = instruction_locator[declared->index];
                if(location.source.kind == compiler::instruction::ExplanationKind::declare) {
                  auto const& parsed_position = output_archive[location.source.index];
                  if(auto* declare_locator = parsed_position.get_if_declare()) {
                    return o << declare_locator->name;
                  }
                }
              } else if(auto* axiom = std::get_if<explanation::Axiom>(&reason)) {
                auto const& location = instruction_locator[axiom->index];
                if(location.source.kind == compiler::instruction::ExplanationKind::axiom) {
                  auto const& parsed_position = output_archive[location.source.index];
                  if(auto* axiom_locator = parsed_position.get_if_axiom()) {
                    return o << axiom_locator->name;
                  }
                }
              }
            }
            return o << "ext_" << ext_index;
          }
        };

        std::cout << "Final: " << fancy_format(expression_context.reduce(eval_result.result.value)) << " of type " << fancy_format(expression_context.reduce(eval_result.result.type)) << "\n";
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

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Vec : Nat -> Type; axiom empty : Vec zero; axiom cons : (n : Nat) -> Nat -> Vec n -> Vec (succ n); declare fold : Nat -> (Nat -> Nat -> Nat) -> (n : Nat) -> Vec n -> Nat; rule fold base combine zero empty = base; rule fold base combine (succ n) (cons n head tail) = fold (combine base head) combine n tail; declare mul : Nat -> Nat -> Nat; declare fst : Nat; fold fst mul _ (cons _ zero (cons _ (succ zero) empty)) }
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
}

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Routine : Type -> Type; axiom return : (T : Type) -> T -> Routine T; axiom read : (T : Type) -> (Nat -> Routine T) -> Routine T; declare bind : (S : Type) -> (T : Type) -> Routine S -> (S -> Routine T) -> Routine T; rule bind S T (return S x) then = then x; rule bind S T (read S f) then = read T \n.bind S T (f n) then; bind }


block { axiom Id : (T : Type) -> T -> T -> Type; axiom refl : (T : Type) -> (x : T) -> Id T x x; declare compose : (T : Type) -> (x : T) -> (y : T) -> (z : T) -> Id T x y -> Id T y z -> Id T x z; rule compose T x x x (refl T x) (refl T x) = refl T x; compose }

...this could be a good use case for coroutines, probably? - let things wait for whatever's blocking their progress
*/
