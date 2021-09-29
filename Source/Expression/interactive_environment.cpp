#include "interactive_environment.hpp"
#include "../CLI/format.hpp"
#include "data_helper.hpp"
#include "expression_debug_format.hpp"
#include "../Utility/result.hpp"
#include "../Compiler/instructions.hpp"
#include "../Compiler/evaluator.hpp"
#include "../ExpressionParser/expression_generator.hpp"
#include "solver.hpp"
#include "standard_solver_context.hpp"
#include "solve_routine.hpp"
#include "formatter.hpp"
#include <sstream>
#include <unordered_map>
#include <map>

namespace expression::interactive {
  namespace {
    struct BaseInfo {
      std::string_view source;
    };
    struct LexInfo : BaseInfo {
      expression_parser::lex_output::archive_root::Term lexer_output;
      expression_parser::lex_locator::archive_root::Term lexer_locator;
    };
    struct ReadInfo : LexInfo {
      expression_parser::output::archive_root::Expression parser_output;
      expression_parser::locator::archive_root::Expression parser_locator;
    };
    struct ResolveInfo : ReadInfo {
      std::vector<TypedValue> embeds;
      expression_parser::resolved::archive_root::Expression parser_resolved;
    };
    struct EvaluateInfo : ResolveInfo {
      compiler::evaluate::EvaluateResult evaluate_result;
      compiler::instruction::output::archive_root::Program instruction_output;
      compiler::instruction::locator::archive_root::Program instruction_locator;
      std::uint64_t rule_begin;
      std::uint64_t rule_end;
      solver::ErrorInfo error_info;

      bool is_solved() const { return error_info.failed_equations.empty() && error_info.unconstrainable_patterns.empty(); }
      std::optional<std::string_view> get_explicit_name(std::uint64_t ext_index) const {
        namespace explanation = compiler::evaluate::variable_explanation;
        if(!evaluate_result.variables.contains(ext_index)) return std::nullopt;
        auto const& reason = evaluate_result.variables.at(ext_index);
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
      std::vector<std::pair<std::string, expression::TypedValue> > get_outer_values() { //values in outermost block, if such a thing makes sense.
        if(!parser_output.root().holds_block()) return {}; //only do anything if block is outermost thing
        auto const& block = parser_output.root().get_block();
        struct IndexHasher { std::size_t operator()(expression_parser::archive_index::PolymorphicKind const& i) const { return i.index(); }};
        std::unordered_map<expression_parser::archive_index::PolymorphicKind, std::string, IndexHasher> outer_block;
        //list which parser tree elements we want to look up
        //ideally, this would be done through forward_locator data generated
        //with the various steps - but for now it's just done with the backwards
        //locator data, since that's implemented :)
        for(auto const& block_element : block.statements) {
          namespace part = expression_parser::output::archive_part;
          block_element.visit(mdb::overloaded{
            [&](part::Declare const& declaration) {
              outer_block.insert(std::make_pair(declaration.index(), declaration.name));
            },
            [&](part::Axiom const& axiom) {
              outer_block.insert(std::make_pair(axiom.index(), axiom.name));
            },
            [&](part::Let const& let) {
              outer_block.insert(std::make_pair(let.index(), let.name));
            },
            [](auto const&) {} //ignore other things (rules)
          });
        }
        //using the locators for evaluator, lookup what corresponds to the prior points
        std::vector<std::pair<std::string, expression::TypedValue> > values;
        for(auto index : instruction_locator.all_indices()) {
          auto const& forward = evaluate_result.forward_locator[index];
          expression::TypedValue const* candidate = forward.visit([](auto const& forward) -> expression::TypedValue const* {
            if constexpr(requires{ forward.result; }) {
              return &forward.result;
            } else {
              return nullptr;
            }
          });
          if(!candidate) continue;
          //now check if we can trace back further
          auto const& location_source = instruction_locator[index].visit([](auto const& v) { return v.source; });
          if(location_source.kind == compiler::instruction::ExplanationKind::declare
          || location_source.kind == compiler::instruction::ExplanationKind::axiom
          || location_source.kind == compiler::instruction::ExplanationKind::let) {
            auto const& parsed_position = parser_output[location_source.index];
            if(outer_block.contains(parsed_position.index())) {
              auto name = outer_block.at(parsed_position.index());
              values.emplace_back(name, *candidate);
            }
          }
        }
        return values;
      }
    };
  }
  struct Environment::Impl {
    expression::Context expression_context;
    expression::data::SmallScalar<std::uint64_t> u64;
    expression::data::SmallScalar<imported_type::StringHolder> str;
    std::unordered_map<std::string, TypedValue> names_to_values;
    std::unordered_map<std::uint64_t, std::string> externals_to_names;
    void name_external(std::string name, std::uint64_t ext) {
      externals_to_names.insert(std::make_pair(ext, name));
      names_to_values.insert(std::make_pair(name, expression_context.get_external(ext)));
    }
    Impl():u64(expression_context), str(expression_context) {
      name_external("Type", expression_context.primitives.type);
      name_external("arrow", expression_context.primitives.arrow);
      name_external("U64", u64.get_type_axiom());
      name_external("String", str.get_type_axiom());
    }
    mdb::Result<LexInfo, std::string> lex_code(BaseInfo input) {
      expression_parser::LexerInfo lexer_info {
        .symbol_map = {
          {"block", 0},
          {"declare", 1},
          {"axiom", 2},
          {"rule", 3},
          {"let", 4},
          {"->", 5},
          {":", 6},
          {";", 7},
          {"=", 8},
          {"\\", 9},
          {"\\\\", 10},
          {".", 11},
          {"_", 12},
          {",", 13}
        }
      };
      auto ret = expression_parser::lex_string(input.source, lexer_info);
      if(auto* success = ret.get_if_value()) {
        return LexInfo{
          input,
          archive(std::move(success->output)),
          archive(std::move(success->locator))
        };
      } else {
        auto const& error = ret.get_error();
        std::stringstream err;
        err << "Error: " << error.message << "\nAt " << format_error(input.source.substr(error.position.begin() - input.source.begin()), input.source);
        return err.str();
      }
    }
    mdb::Result<ReadInfo, std::string> read_code(LexInfo input) {
      auto ret = expression_parser::parse_lexed(input.lexer_output.root());
      if(auto* success = ret.get_if_value()) {
        return ReadInfo{
          std::move(input),
          archive(std::move(success->output)),
          archive(std::move(success->locator))
        };
      } else {
        auto const& error = ret.get_error();
        std::stringstream err;
        err << "Error: " << error.message << "\nAt " << format_error(expression_parser::position_of(input.lexer_locator[error.position]), input.source);
        return err.str();
      }
    }
    mdb::Result<ResolveInfo, std::string> resolve(ReadInfo input) {
      std::vector<TypedValue> embeds;
      std::unordered_map<std::string, std::uint64_t> names_to_embeds;
      auto resolved = expression_parser::resolve(expression_parser::resolved::ContextLambda {
        [&](std::string_view str) -> std::optional<std::uint64_t> { //lookup
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
        [&](auto const& literal) -> std::uint64_t { //embed_literal
          auto ret = embeds.size();
          std::visit(mdb::overloaded{
            [&](std::uint64_t literal) {
              auto ret = u64(literal);
              auto t = ret.get_data().data.type_of();
              embeds.push_back({std::move(ret), std::move(t)});
            },
            [&](std::string literal) {
              auto ret = str(imported_type::StringHolder{literal});
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
          err_out << "Bad id: " << format_error(expression_parser::position_of(bad_pos, input.lexer_locator), input.source) << "\n";
        }
        for(auto const& bad_id : err.bad_pattern_ids) {
          auto bad_pos = input.parser_locator[bad_id].position;
          err_out << "Bad pattern id: " << format_error(expression_parser::position_of(bad_pos, input.lexer_locator), input.source) << "\n";
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
      auto hung_equations = solve_routine.get_errors();

      return EvaluateInfo{
        std::move(input),
        std::move(eval_result), //copies because... well... someone apparently cares about the result?
        std::move(instruction_output),
        std::move(instruction_locator),
        rule_start,
        rule_end,
        std::move(hung_equations)
      };
    }
    mdb::Result<EvaluateInfo, std::string> full_compile(std::string_view str) {
      return map(bind(
        lex_code({str}),
        [this](auto last) { return read_code(std::move(last)); },
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
        auto ret = expression_context.reduce(std::move(value->evaluate_result.result.value));
        auto ret_type = expression_context.reduce(std::move(value->evaluate_result.result.type));
        if(!value->is_solved()) {
          std::cerr << "While compiling: " << expr << "\n";
          std::cerr << "Solving failed.\n";
          auto fancy = fancy_format(*value);
          /*for(auto const& eq : value->remaining_equations) {
            std::cout << fancy(eq.lhs, eq.depth) << (eq.failed ? " =!= " : " =?= ") << fancy(eq.rhs, eq.depth) << "\n";
          }*/ //ignore for now
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
    void debug_parse(std::string_view expr, std::ostream& output) {
      auto compile = full_compile(expr);
      if(auto* value = compile.get_if_value()) {
        /*
        This block prints a list of variables. Should be factored out to to show this information as needed
        instead of just in a block. (Possibly with an option to print the whole blog for debugging?)
*/
        std::map<std::uint64_t, compiler::evaluate::variable_explanation::Any> sorted_variables;
        for(auto const& entry : value->evaluate_result.variables) sorted_variables.insert(entry);
        for(auto const& [var, reason] : sorted_variables) {
          output << std::visit(mdb::overloaded{
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
          output << "Position: " << format_info(expression_parser::position_of(str_pos, value->lexer_locator), value->source) << "\n";
        }
        output << "\n";
        for(auto const& unconstrainable : value->error_info.unconstrainable_patterns) {
          auto const& reason = value->evaluate_result.rule_explanations[unconstrainable.rule_index];
          auto const& pos = value->instruction_locator[reason.index];
          auto const& locator_index = pos.source.index;
          auto const& locator_pos = value->parser_locator[locator_index];
          output << "Proposed rule could not have its pattern represented in a suitable form: ";
          if(locator_pos.holds_rule()) {
            auto const& lhs_pos = locator_pos.get_rule().pattern.visit([&](auto const& o) { return o.position; });
            auto const& rhs_pos = locator_pos.get_rule().replacement.visit([&](auto const& o) { return o.position; });
            output << format_info_pair(
              expression_parser::position_of(lhs_pos, value->lexer_locator),
              expression_parser::position_of(rhs_pos, value->lexer_locator),
              value->source
            );
          } else {
            //this shouldn't happen - rules *can* come from non-rule locators, but those shouldn't ever fail.
            auto const& pos = locator_pos.visit([&](auto const& o) { return o.position; });
            output << format_info(
              expression_parser::position_of(pos, value->lexer_locator),
              value->source
            );
          }
          output << "\n";
        }
        for(auto const& eq : value->error_info.failed_equations) {
          if(eq.failed) {
            output << red_string("False Equation: ");
          } else {
            output << yellow_string("Undetermined Equation: ");
          }
          auto depth = eq.info.primary.stack.depth();
          auto fancy = fancy_format(*value);
          output << fancy(eq.info.primary.lhs, depth) << (eq.failed ? " =!= " : " =?= ") << fancy(eq.info.primary.rhs, depth) << "\n";
          for(auto const& secondary_fail : eq.info.secondary_fail) {
            auto depth = secondary_fail.stack.depth();
            output << grey_string("Failure in sub-equation: ") << fancy(secondary_fail.lhs, depth) << " =!= " << fancy(secondary_fail.rhs, depth) << "\n";
          }
          for(auto const& secondary_stuck : eq.info.secondary_stuck) {
            auto depth = secondary_stuck.stack.depth();
            output << grey_string("Stuck in sub-equation: ") << fancy(secondary_stuck.lhs, depth) << " =!= " << fancy(secondary_stuck.rhs, depth) << "\n";
          }

          if(eq.source_kind == solver::SourceKind::cast_equation) {
            auto const& cast = value->evaluate_result.casts[eq.source_index];
            auto cast_var = cast.variable;
            if(value->evaluate_result.variables.contains(cast_var)) {
              auto const& reason = value->evaluate_result.variables.at(cast_var);
              bool is_apply_cast = false;
              auto reason_string = std::visit(mdb::overloaded{
                [&](compiler::evaluate::variable_explanation::ApplyRHSCast const&) { is_apply_cast = true; return "While matching RHS to domain type in application: "; },
                [&](compiler::evaluate::variable_explanation::ApplyLHSCast const&) { is_apply_cast = true; return "While matching LHS to function type in application: "; },
                [&](compiler::evaluate::variable_explanation::TypeFamilyCast const&) { return "While matching the type of a type family: "; },
                [&](compiler::evaluate::variable_explanation::HoleTypeCast const&) { return "While matching the type of a hole: "; },
                [&](compiler::evaluate::variable_explanation::DeclareTypeCast const&) { return "While matching the declaration type against Type: "; },
                [&](compiler::evaluate::variable_explanation::AxiomTypeCast const&) { return "While matching the axiom type against Type: "; },
                [&](compiler::evaluate::variable_explanation::LetTypeCast const&) { return "While matching the let type against Type: "; },
                [&](compiler::evaluate::variable_explanation::LetCast const&) { return "While matching the declared type of the let with the expression type: "; },
                [&](compiler::evaluate::variable_explanation::ForAllTypeCast const&) { return "While matching the for all type against Type: "; },
                [&](auto const&) { return "For unknown reasons: "; }
              }, reason);
              auto const& index = std::visit([&](auto const& reason) -> compiler::instruction::archive_index::PolymorphicKind {
                return reason.index;
              }, reason);
              auto const& pos = value->instruction_locator[index];
              auto const& locator_index = pos.visit([&](auto const& obj) { return obj.source.index; });
              auto const& locator_pos = value->parser_locator[locator_index];
              if(is_apply_cast && locator_pos.holds_apply()) {
                auto const& apply = locator_pos.get_apply();
                auto const& lhs_pos = apply.lhs.visit([&](auto const& o) { return o.position; });
                auto const& rhs_pos = apply.rhs.visit([&](auto const& o) { return o.position; });
                output << reason_string << format_info_pair(
                  expression_parser::position_of(lhs_pos, value->lexer_locator),
                  expression_parser::position_of(rhs_pos, value->lexer_locator),
                  value->source
                );
              } else {
                auto const& str_pos = locator_pos.visit([&](auto const& o) { return o.position; });
                output << reason_string << format_info(expression_parser::position_of(str_pos, value->lexer_locator), value->source);
              }
            } else {
              output << "From cast #" << eq.source_index << ". Could not be located.";
            }
          } else if(eq.source_index != -1) {
            auto const& reason = value->evaluate_result.rule_explanations[eq.source_index];
            auto const& pos = value->instruction_locator[reason.index];
            auto const& locator_index = pos.source.index;
            auto const& locator_pos = value->parser_locator[locator_index];
            switch(eq.source_kind) {
              case solver::SourceKind::rule_equation:
                output << "While checking the LHS and RHS of rule have same type: "; break;
              case solver::SourceKind::rule_skeleton:
                output << "While deducing relations among the capture-point skeleton of: "; break;
              case solver::SourceKind::rule_skeleton_verify:
                output << "While checking satisfaction of relations amount capture-point skeleton of: "; break;
              default:
                output << "While doing unknown task with rule: "; break;
            }
            if(locator_pos.holds_rule()) {
              auto const& lhs_pos = locator_pos.get_rule().pattern.visit([&](auto const& o) { return o.position; });
              auto const& rhs_pos = locator_pos.get_rule().replacement.visit([&](auto const& o) { return o.position; });
              output << format_info_pair(
                expression_parser::position_of(lhs_pos, value->lexer_locator),
                expression_parser::position_of(rhs_pos, value->lexer_locator),
                value->source
              );
            } else {
              //this shouldn't be reachable
              auto const& pos = locator_pos.visit([&](auto const& o) { return o.position; });
              output << format_info(
                expression_parser::position_of(pos, value->lexer_locator),
                value->source
              );
            }
          } else {
            output << "Unknown source.";
          }
          output << "\n\n";
        }
        auto fancy = fancy_format(*value); //get the formatters
        auto deep = deep_format(*value);
        /*
        This block prints every new rule.

        std::vector<expression::Rule> new_rules;
        for(auto i = value->rule_begin; i < value->rule_end; ++i) {
          new_rules.push_back(expression_context.rules[i]);
        }
        std::sort(new_rules.begin(), new_rules.end(), [](auto const& lhs, auto const& rhs) {
          return get_pattern_head(lhs.pattern) < get_pattern_head(rhs.pattern);
        });
        for(auto const& rule : new_rules) {
          output << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
        }
        output << "\n";
        */

        //throw new variables into context
        for(auto const& entry : value->get_outer_values()) {
          //output << entry.first << " : " << fancy_format(*value)(entry.second.type) << "\n";
          names_to_values.insert_or_assign(entry.first, entry.second);
        }
        for(auto const& var_data : value->evaluate_result.variables) {
          auto const& var_index = var_data.first;
          if(auto str = value->get_explicit_name(var_index)) {
            externals_to_names.insert(std::make_pair(var_index, std::move(*str)));
          }
        }
        output << "Final: " << fancy_format(*value)(value->evaluate_result.result.value) << " of type " << fancy_format(*value)(value->evaluate_result.result.type) << "\n";
        output << "Deep: " << deep_format(*value)(value->evaluate_result.result.value) << " of type " << deep_format(*value)(value->evaluate_result.result.type) << "\n";
      } else {
        output << compile.get_error() << "\n";
      }
    }
    ParseResult parse(std::string_view expr);
    bool deep_compare(tree::Expression lhs, tree::Expression rhs) {
      solver::StandardSolverContext context{expression_context};
      solver::Solver solver{context};
      auto result = solver.solve({
        .equation = {
          .stack = Stack::empty(expression_context),
          .lhs = std::move(lhs),
          .rhs = std::move(rhs)
        }
      });
      solver.try_to_make_progress();
      solver.close();
      return !std::move(result).take();
    }
    bool deep_compare(TypedValue lhs, TypedValue rhs) {
      return deep_compare(std::move(lhs.value), std::move(rhs.value))
          && deep_compare(std::move(lhs.type), std::move(rhs.type));
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
  void Environment::debug_parse(std::string_view str, std::ostream& output) {
    return impl->debug_parse(str, output);
  }
  ParseResult Environment::parse(std::string_view str) {
    return impl->parse(str);
  }
  void Environment::name_external(std::string name, std::uint64_t external) {
    return impl->name_external(std::move(name), external);
  }
  Context& Environment::context() { return impl->expression_context; }
  expression::data::SmallScalar<std::uint64_t> const& Environment::u64() const { return impl->u64; }
  expression::data::SmallScalar<imported_type::StringHolder> const& Environment::str() const { return impl->str; }
  bool Environment::deep_compare(tree::Expression lhs, tree::Expression rhs) const { return impl->deep_compare(std::move(lhs), std::move(rhs)); }
  bool Environment::deep_compare(TypedValue lhs, TypedValue rhs) const { return impl->deep_compare(std::move(lhs), std::move(rhs)); }

  struct ParseResult::Impl {
    Environment::Impl* environment;
    std::variant<std::string, EvaluateInfo> data;
    bool has_result() const {
      return data.index() == 1;
    }
    EvaluateInfo& info() { return std::get<1>(data); }
    EvaluateInfo const& info() const { return std::get<1>(data); }
    bool is_fully_solved() const {
      return has_result() && info().is_solved();
    }
    TypedValue const& get_result() const {
      return info().evaluate_result.result;
    }
    TypedValue get_reduced_result() const {
      return {
        .value = environment->expression_context.reduce(get_result().value),
        .type = environment->expression_context.reduce(get_result().type)
      };
    }
    void print_errors_to(std::ostream& output) const {
      if(auto* error_str = std::get_if<0>(&data)) {
        output << *error_str;
        return;
      }
      for(auto const& unconstrainable : info().error_info.unconstrainable_patterns) {
        auto const& reason = info().evaluate_result.rule_explanations[unconstrainable.rule_index];
        auto const& pos = info().instruction_locator[reason.index];
        auto const& locator_index = pos.source.index;
        auto const& locator_pos = info().parser_locator[locator_index];
        output << "Proposed rule could not have its pattern represented in a suitable form: ";
        if(locator_pos.holds_rule()) {
          auto const& lhs_pos = locator_pos.get_rule().pattern.visit([&](auto const& o) { return o.position; });
          auto const& rhs_pos = locator_pos.get_rule().replacement.visit([&](auto const& o) { return o.position; });
          output << format_info_pair(
            expression_parser::position_of(lhs_pos, info().lexer_locator),
            expression_parser::position_of(rhs_pos, info().lexer_locator),
            info().source
          );
        } else {
          //this shouldn't happen - rules *can* come from non-rule locators, but those shouldn't ever fail.
          auto const& pos = locator_pos.visit([&](auto const& o) { return o.position; });
          output << format_info(
            expression_parser::position_of(pos, info().lexer_locator),
            info().source
          );
        }
        output << "\n";
      }
      for(auto const& eq : info().error_info.failed_equations) {
        if(eq.failed) {
          output << red_string("False Equation: ");
        } else {
          output << yellow_string("Undetermined Equation: ");
        }
        auto depth = eq.info.primary.stack.depth();
        auto fancy = environment->fancy_format(info());
        output << fancy(eq.info.primary.lhs, depth) << (eq.failed ? " =!= " : " =?= ") << fancy(eq.info.primary.rhs, depth) << "\n";
        for(auto const& secondary_fail : eq.info.secondary_fail) {
          auto depth = secondary_fail.stack.depth();
          output << grey_string("Failure in sub-equation: ") << fancy(secondary_fail.lhs, depth) << " =!= " << fancy(secondary_fail.rhs, depth) << "\n";
        }
        for(auto const& secondary_stuck : eq.info.secondary_stuck) {
          auto depth = secondary_stuck.stack.depth();
          output << grey_string("Stuck in sub-equation: ") << fancy(secondary_stuck.lhs, depth) << " =!= " << fancy(secondary_stuck.rhs, depth) << "\n";
        }

        if(eq.source_kind == solver::SourceKind::cast_equation) {
          auto const& cast = info().evaluate_result.casts[eq.source_index];
          auto cast_var = cast.variable;
          if(info().evaluate_result.variables.contains(cast_var)) {
            auto const& reason = info().evaluate_result.variables.at(cast_var);
            bool is_apply_cast = false;
            auto reason_string = std::visit(mdb::overloaded{
              [&](compiler::evaluate::variable_explanation::ApplyRHSCast const&) { is_apply_cast = true; return "While matching RHS to domain type in application: "; },
              [&](compiler::evaluate::variable_explanation::ApplyLHSCast const&) { is_apply_cast = true; return "While matching LHS to function type in application: "; },
              [&](compiler::evaluate::variable_explanation::TypeFamilyCast const&) { return "While matching the type of a type family: "; },
              [&](compiler::evaluate::variable_explanation::HoleTypeCast const&) { return "While matching the type of a hole: "; },
              [&](compiler::evaluate::variable_explanation::DeclareTypeCast const&) { return "While matching the declaration type against Type: "; },
              [&](compiler::evaluate::variable_explanation::AxiomTypeCast const&) { return "While matching the axiom type against Type: "; },
              [&](compiler::evaluate::variable_explanation::LetTypeCast const&) { return "While matching the let type against Type: "; },
              [&](compiler::evaluate::variable_explanation::LetCast const&) { return "While matching the declared type of the let with the expression type: "; },
              [&](compiler::evaluate::variable_explanation::ForAllTypeCast const&) { return "While matching the for all type against Type: "; },
              [&](auto const&) { return "For unknown reasons: "; }
            }, reason);
            auto const& index = std::visit([&](auto const& reason) -> compiler::instruction::archive_index::PolymorphicKind {
              return reason.index;
            }, reason);
            auto const& pos = info().instruction_locator[index];
            auto const& locator_index = pos.visit([&](auto const& obj) { return obj.source.index; });
            auto const& locator_pos = info().parser_locator[locator_index];
            if(is_apply_cast && locator_pos.holds_apply()) {
              auto const& apply = locator_pos.get_apply();
              auto const& lhs_pos = apply.lhs.visit([&](auto const& o) { return o.position; });
              auto const& rhs_pos = apply.rhs.visit([&](auto const& o) { return o.position; });
              output << reason_string << format_info_pair(
                expression_parser::position_of(lhs_pos, info().lexer_locator),
                expression_parser::position_of(rhs_pos, info().lexer_locator),
                info().source
              );
            } else {
              auto const& str_pos = locator_pos.visit([&](auto const& o) { return o.position; });
              output << reason_string << format_info(expression_parser::position_of(str_pos, info().lexer_locator), info().source);
            }
          } else {
            output << "From cast #" << eq.source_index << ". Could not be located.";
          }
        } else if(eq.source_index != -1) {
          auto const& reason = info().evaluate_result.rule_explanations[eq.source_index];
          auto const& pos = info().instruction_locator[reason.index];
          auto const& locator_index = pos.source.index;
          auto const& locator_pos = info().parser_locator[locator_index];
          switch(eq.source_kind) {
            case solver::SourceKind::rule_equation:
              output << "While checking the LHS and RHS of rule have same type: "; break;
            case solver::SourceKind::rule_skeleton:
              output << "While deducing relations among the capture-point skeleton of: "; break;
            case solver::SourceKind::rule_skeleton_verify:
              output << "While checking satisfaction of relations amount capture-point skeleton of: "; break;
            default:
              output << "While doing unknown task with rule: "; break;
          }
          if(locator_pos.holds_rule()) {
            auto const& lhs_pos = locator_pos.get_rule().pattern.visit([&](auto const& o) { return o.position; });
            auto const& rhs_pos = locator_pos.get_rule().replacement.visit([&](auto const& o) { return o.position; });
            output << format_info_pair(
              expression_parser::position_of(lhs_pos, info().lexer_locator),
              expression_parser::position_of(rhs_pos, info().lexer_locator),
              info().source
            );
          } else {
            //this shouldn't be reachable
            auto const& pos = locator_pos.visit([&](auto const& o) { return o.position; });
            output << format_info(
              expression_parser::position_of(pos, info().lexer_locator),
              info().source
            );
          }
        } else {
          output << "Unknown source.";
        }
        output << "\n\n";
      }
    }
    void print_value(std::ostream& output, tree::Expression value) {
      auto fancy = environment->fancy_format(info());
      output << fancy(value);
    }
  };
  ParseResult Environment::Impl::parse(std::string_view expr) {
    auto compile = full_compile(expr);
    if(auto* value = compile.get_if_value()) {
      return ParseResult{std::unique_ptr<ParseResult::Impl>{new ParseResult::Impl{
        .environment = this,
        .data = std::move(*value)
      }}};
    } else {
      return ParseResult{std::unique_ptr<ParseResult::Impl>{new ParseResult::Impl{
        .environment = this,
        .data = std::move(compile.get_error())
      }}};
    }
  }
  ParseResult::ParseResult(std::unique_ptr<Impl> impl):impl(std::move(impl)) {}
  ParseResult::ParseResult(ParseResult&&) = default;
  ParseResult& ParseResult::operator=(ParseResult&&) = default;
  ParseResult::~ParseResult() = default;
  bool ParseResult::has_result() const { return impl->has_result(); }
  bool ParseResult::is_fully_solved() const { return impl->is_fully_solved(); }
  TypedValue const& ParseResult::get_result() const { return impl->get_result(); }
  TypedValue ParseResult::get_reduced_result() const { return impl->get_reduced_result(); }
  void ParseResult::print_errors_to(std::ostream& output) const{ return impl->print_errors_to(output); }
}
