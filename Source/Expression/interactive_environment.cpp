#include "interactive_environment.hpp"
#include "../CLI/format.hpp"
#include "data_helper.hpp"
#include "../ExpressionParser/expression_parser.hpp"
#include "expression_debug_format.hpp"
#include "../Utility/result.hpp"
#include "../Compiler/instructions.hpp"
#include "../Compiler/evaluator.hpp"
#include "solver.hpp"
#include "standard_solver_context.hpp"
#include "solve_routine.hpp"
#include "formatter.hpp"
#include <sstream>
#include <unordered_map>
#include <map>

namespace expression::interactive {
  struct Environment::Impl {
    expression::Context expression_context;
    expression::data::SmallScalar<std::uint64_t> u64;
    expression::data::SmallScalar<StrHolder> str;
    std::unordered_map<std::string, TypedValue> names_to_values;
    std::unordered_map<std::uint64_t, std::string> externals_to_names;
    Impl():u64(expression_context), str(expression_context) {
      auto register_primitive = [&](std::string name, std::uint64_t ext) {
        externals_to_names.insert(std::make_pair(ext, name));
        names_to_values.insert(std::make_pair(name, expression_context.get_external(ext)));
      };
      register_primitive("Type", expression_context.primitives.type);
      register_primitive("arrow", expression_context.primitives.arrow);
      register_primitive("U64", u64.get_type_axiom());
      register_primitive("String", str.get_type_axiom());
    }
    struct BaseInfo {
      std::string_view source;
    };
    struct ReadInfo : BaseInfo {
      expression_parser::output::archive_root::Expression parser_output;
      expression_parser::locator::archive_root::Expression parser_locator;
    };
    struct ResolveInfo : ReadInfo {
      std::vector<TypedValue> embeds;
      expression_parser::resolved::archive_root::Expression parser_resolved;
    };
    struct EvaluateInfo : ResolveInfo {
      compiler::instruction::output::archive_root::Program instruction_output;
      compiler::instruction::locator::archive_root::Program instruction_locator;
      std::unordered_map<std::uint64_t, compiler::evaluate::variable_explanation::Any> variable_explanations;
      std::uint64_t rule_begin;
      std::uint64_t rule_end;
      std::vector<std::pair<tree::Expression, tree::Expression> > remaining_equations;
      TypedValue value;

      bool is_solved() const { return remaining_equations.empty(); }
      std::optional<std::string_view> get_explicit_name(std::uint64_t ext_index) const {
        namespace explanation = compiler::evaluate::variable_explanation;
        if(!variable_explanations.contains(ext_index)) return std::nullopt;
        auto const& reason = variable_explanations.at(ext_index);
        if(auto* declared = std::get_if<explanation::Declaration>(&reason)) {
          auto const& location = instruction_locator[declared->index];
          if(location.source.kind == compiler::instruction::ExplanationKind::declare) {
            auto const& parsed_position = parser_output[location.source.index];
            if(auto* declare_locator = parsed_position.get_if_declare()) {
              return declare_locator->name;
            }
          }
        } else if(auto* axiom = std::get_if<explanation::Axiom>(&reason)) {
          auto const& location = instruction_locator[axiom->index];
          if(location.source.kind == compiler::instruction::ExplanationKind::axiom) {
            auto const& parsed_position = parser_output[location.source.index];
            if(auto* axiom_locator = parsed_position.get_if_axiom()) {
              return axiom_locator->name;
            }
          }
        }
        return std::nullopt;
      }
    };
    mdb::Result<ReadInfo, std::string> read_code(BaseInfo input) {
      auto ret = expression_parser::parser::expression(input.source);
      if(auto* success = parser::get_if_success(&ret)) {
        return ReadInfo{
          input,
          archive(std::move(success->value.output)),
          archive(std::move(success->value.locator))
        };
      } else {
        auto const& error = parser::get_error(ret);
        std::stringstream err;
        err << "Error: " << error.error << "\nAt " << format_error(input.source.substr(error.position.begin() - input.source.begin()), input.source);
        return err.str();
      }
    }
    mdb::Result<ResolveInfo, std::string> resolve(ReadInfo input) {
      std::vector<TypedValue> embeds;
      std::unordered_map<std::string, std::uint64_t> names_to_embeds;
      auto resolved = expression_parser::resolve(expression_parser::resolved::ContextLambda {
        .lookup = [&](std::string_view str) -> std::optional<std::uint64_t> {
          std::string s{str};
          if(names_to_embeds.contains(s)) {
            return names_to_embeds.at(s);
          } else if(names_to_values.contains(s)) {
            auto index = embeds.size();
            embeds.push_back(names_to_values.at(s));
            names_to_embeds.insert(std::make_pair(std::move(s), index));
            return index;
          } else {
            return std::nullopt;
          }
        },
        .embed_literal = [&](auto const& literal) -> std::uint64_t {
          auto ret = embeds.size();
          std::visit(mdb::overloaded{
            [&](std::uint64_t literal) {
              auto ret = u64(literal);
              auto t = ret.get_data().data.type_of();
              embeds.push_back({std::move(ret), std::move(t)});
            },
            [&](std::string literal) {
              auto ret = str(StrHolder{std::make_shared<std::string>(std::move(literal))});
              auto t = ret.get_data().data.type_of();
              embeds.push_back({std::move(ret), std::move(t)});
            }
          }, literal);
          return ret;
        }
      }, input.parser_output.root());
      if(auto* resolve = resolved.get_if_value()) {
        return ResolveInfo{
          std::move(input),
          std::move(embeds),
          archive(std::move(*resolve))
        };
      } else {
        std::stringstream err_out;
        auto const& err = resolved.get_error();
        for(auto const& bad_id : err.bad_ids) {
          auto bad_pos = input.parser_locator[bad_id].position;
          err_out << "Bad id: " << format_error(bad_pos, input.source) << "\n";
        }
        for(auto const& bad_id : err.bad_pattern_ids) {
          auto bad_pos = input.parser_locator[bad_id].position;
          err_out << "Bad pattern id: " << format_error(bad_pos, input.source) << "\n";
        }
        return err_out.str();
      }
    }
    EvaluateInfo evaluate(ResolveInfo input) {
      auto rule_start = expression_context.rules.size();
      auto instructions = compiler::instruction::make_instructions(input.parser_resolved.root());
      auto instruction_output = archive(std::move(instructions.output));
      auto instruction_locator = archive(std::move(instructions.locator));
      auto eval_result = compiler::evaluate::evaluate_tree(instruction_output.root().get_program_root(), expression_context, [&](std::uint64_t embed_index) {
        return input.embeds.at(embed_index);
      });
      expression::solver::StandardSolverContext solver_context {
        .evaluation = expression_context
      };
      expression::solver::Routine solve_routine{eval_result, expression_context, solver_context};
      solve_routine.run();
      auto rule_end = expression_context.rules.size();

      return EvaluateInfo{
        std::move(input),
        std::move(instruction_output),
        std::move(instruction_locator),
        std::move(eval_result.variables),
        rule_start,
        rule_end,
        solve_routine.get_equations(),
        std::move(eval_result.result)
      };
    }
    mdb::Result<EvaluateInfo, std::string> full_compile(std::string_view str) {
      return map(bind(
        read_code({str}),
        [this](auto last) { return resolve(std::move(last)); }
      ), [this](auto last) { return evaluate(std::move(last)); });
    }
    auto fancy_format(EvaluateInfo const& eval_info) {
      return expression::format::FormatContext{
        .expression_context = expression_context,
        .force_expansion = [&](std::uint64_t ext_index){ return !externals_to_names.contains(ext_index) && !eval_info.get_explicit_name(ext_index); },
        .write_external = [&](std::ostream& o, std::uint64_t ext_index) -> std::ostream& {
          if(externals_to_names.contains(ext_index)) {
            return o << externals_to_names.at(ext_index);
          } else if(auto name = eval_info.get_explicit_name(ext_index)) {
            return o << *name;
          } else {
            return o << "ext_" << ext_index;
          }
        }
      };
    }
    auto deep_format(EvaluateInfo const& eval_info) {
      return expression::format::FormatContext{
        .expression_context = expression_context,
        .force_expansion = [&](std::uint64_t ext_index){ return true; },
        .write_external = [&](std::ostream& o, std::uint64_t ext_index) -> std::ostream& {
          if(externals_to_names.contains(ext_index)) {
            return o << externals_to_names.at(ext_index);
          } else if(auto name = eval_info.get_explicit_name(ext_index)) {
            return o << *name;
          } else {
            return o << "ext_" << ext_index;
          }
        }
      };
    }

    DeclarationInfo declare_or_axiom_check(std::string name, std::string_view expr, bool axiom) {
      auto compile = full_compile(expr);
      if(auto* value = compile.get_if_value()) {
        auto ret = expression_context.reduce(std::move(value->value.value));
        auto ret_type = expression_context.reduce(std::move(value->value.type));
        if(!value->is_solved()) {
          std::cerr << "While compiling: " << expr << "\n";
          std::cerr << "Solving failed.\n";
          auto fancy = fancy_format(*value);
          for(auto const& [lhs, rhs] : value->remaining_equations) {
            std::cout << fancy(lhs) << " =?= " << fancy(rhs) << "\n";
          }
          std::terminate();
        }
        if(ret_type != tree::Expression{tree::External{expression_context.primitives.type}}) {
          std::cerr << "While compiling: " << expr << "\n";
          std::cerr << "Type was not Type.\n";
          std::terminate();
        }
        auto declaration = expression_context.create_variable({
          .is_axiom = axiom,
          .type = std::move(ret)
        });
        auto typed_ret = expression_context.get_external(declaration);
        if(!name.empty()) {
          if(names_to_values.contains(name)) {
            std::cerr << "While compiling: " << expr << "\n";
            std::cerr << name << " is already defined.\n";
            std::terminate();
          }
          names_to_values.insert(std::make_pair(name, typed_ret));
          externals_to_names.insert(std::make_pair(declaration, name));
        }
        return DeclarationInfo{declaration, std::move(typed_ret)};
      } else {
        std::cerr << "While compiling: " << expr << "\n";
        std::cerr << compile.get_error() << "\n";
        std::terminate();
      }
    }
    void debug_parse(std::string_view expr) {
      auto compile = full_compile(expr);
      if(auto* value = compile.get_if_value()) {
        std::map<std::uint64_t, compiler::evaluate::variable_explanation::Any> sorted_variables;
        for(auto const& entry : value->variable_explanations) sorted_variables.insert(entry);
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
          auto const& pos = value->instruction_locator[index];
          auto const& locator_index = pos.visit([&](auto const& obj) { return obj.source.index; });
          auto const& locator_pos = value->parser_locator[locator_index];
          auto const& str_pos = locator_pos.visit([&](auto const& o) { return o.position; });
          std::cout << "Position: " << format_info(str_pos, value->source) << "\n";
        }
        std::vector<expression::Rule> new_rules;
        for(auto i = value->rule_begin; i < value->rule_end; ++i) {
          new_rules.push_back(expression_context.rules[i]);
        }
        std::sort(new_rules.begin(), new_rules.end(), [](auto const& lhs, auto const& rhs) {
          return get_pattern_head(lhs.pattern) < get_pattern_head(rhs.pattern);
        });
        auto fancy = fancy_format(*value);
        auto deep = deep_format(*value);

        for(auto const& rule : new_rules) {
          std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
        }
        for(auto const& [lhs, rhs] : value->remaining_equations) {
          std::cout << fancy(lhs) << " =?= " << fancy(rhs) << "\n";
        }
        std::cout << "Final: " << fancy_format(*value)(value->value.value) << " of type " << fancy_format(*value)(value->value.type) << "\n";
        std::cout << "Deep: " << deep_format(*value)(value->value.value) << " of type " << deep_format(*value)(value->value.type) << "\n";
      } else {
        std::cerr << "While compiling: " << expr << "\n";
        std::cerr << compile.get_error() << "\n";
      }
    }

  };

  Environment::Environment():impl(std::make_unique<Impl>()) {}
  Environment::Environment(Environment&&) = default;
  Environment& Environment::operator=(Environment&&) = default;
  Environment::~Environment() = default;

  Environment::DeclarationInfo Environment::declare_check(std::string_view expr) {
    return impl->declare_or_axiom_check("", expr, false);
  }
  Environment::DeclarationInfo Environment::declare_check(std::string name, std::string_view expr) {
    return impl->declare_or_axiom_check(std::move(name), expr, false);
  }
  Environment::DeclarationInfo Environment::axiom_check(std::string_view expr) {
    return impl->declare_or_axiom_check("", expr, true);
  }
  Environment::DeclarationInfo Environment::axiom_check(std::string name, std::string_view expr) {
    return impl->declare_or_axiom_check(std::move(name), expr, true);
  }
  void Environment::debug_parse(std::string_view str) {
    return impl->debug_parse(str);
  }

  Context& Environment::context() { return impl->expression_context; }
  expression::data::SmallScalar<std::uint64_t> const& Environment::u64() const { return impl->u64; }
  expression::data::SmallScalar<StrHolder> const& Environment::str() const { return impl->str; }
}
