#include "pipeline.hpp"
#include "../CLI/format.hpp"

namespace pipeline::compile {
  std::string_view EvaluateInfo::locate(compiler::new_instruction::archive_index::PolymorphicKind index) const {
    auto parser_index = instruction_locator[index].visit([](auto const& locator) { return locator.source.index; });
    auto lexer_span = parser_locator[parser_index].visit([](auto const& locator) { return locator.position; });
    return expression_parser::position_of(expression_parser::LexerLocatorSpan{
      lexer_span,
      lexer_locator
    });
  }
  std::optional<std::string_view> EvaluateInfo::get_explicit_name_of(new_expression::WeakExpression primitive) const {
    namespace explanation = solver::evaluator::variable_explanation;
    if(!variable_explanations.contains(primitive)) return std::nullopt;
    auto const& reason = variable_explanations.at(primitive);
    if(auto* declared = std::get_if<explanation::Declaration>(&reason)) {
      auto const& location = instruction_locator[declared->index];
      if(location.source.kind == compiler::new_instruction::ExplanationKind::declare) {
        auto const& parsed_position = parser_output[location.source.index];
        if(auto* declare_locator = parsed_position.get_if_declare()) {
          return declare_locator->name;
        }
      }
    } else if(auto* axiom = std::get_if<explanation::Axiom>(&reason)) {
      auto const& location = instruction_locator[axiom->index];
      if(location.source.kind == compiler::new_instruction::ExplanationKind::axiom) {
        auto const& parsed_position = parser_output[location.source.index];
        if(auto* axiom_locator = parsed_position.get_if_axiom()) {
          return axiom_locator->name;
        }
      }
    }
    return std::nullopt;
  }
  void EvaluateInfo::report_errors_to(std::ostream& o, new_expression::WeakKeyMap<std::string> const& extra_names) const {
    auto report = [&](std::string_view text, compiler::new_instruction::archive_index::PolymorphicKind index) {
      o << "Error: " << text << "\n" << format_error(locate(index), source) << "\n";
    };
    auto report_double = [&](std::string_view text, compiler::new_instruction::archive_index::PolymorphicKind index, compiler::new_instruction::archive_index::PolymorphicKind index2) {
      o << "Error: " << text << "\n" << format_info_pair(locate(index), locate(index2), source) << "\n";
    };
    auto report_eq = [&](solver::EquationErrorInfo const& err) {
      auto namer = [&](std::ostream& o, new_expression::WeakExpression expr) {
        if(auto name = get_explicit_name_of(expr)) {
          o << *name;
        } else if(extra_names.contains(expr)) {
          o << extra_names.at(expr);
        } else if(arena.holds_declaration(expr)) {
          o << "decl_" << expr.data();
        } else if(arena.holds_axiom(expr)) {
          o << "ax_" << expr.data();
        } else {
          o << "???";
        }
      };
      auto format_expr = [&](new_expression::WeakExpression expr) {
        return user::raw_format(arena, expr, namer);
      };
      auto print_stack = [&](stack::Stack const& stack) {
        auto assumptions = stack.list_assumptions();
        for(auto const& assumption : assumptions.assumptions) {
          o << "Given " << format_expr(assumption.representative);
          for(auto const& term : assumption.terms) {
            o << " = " << format_expr(term);
          }
          o << "\n";
        }
        destroy_from_arena(arena, assumptions);
      };
      auto print_eq = [&](solver::Equation const& eq, bool fail) {
        if(fail) o << "Failed:\n";
        else o << "Stalled:\n";
        o << format_expr(eq.lhs) << " =?= " << format_expr(eq.rhs) << "\n";
        print_stack(eq.stack);
      };
      print_eq(err.primary, err.primary_failed);
      for(auto const& eq : err.failures) print_eq(eq, true);
      for(auto const& eq : err.stalls) print_eq(eq, false);
    };
    for(auto const& error : evaluation_errors) {
      using namespace solver::evaluator::error;
      std::visit(mdb::overloaded{
        [&](NotAFunction const& err) { report("Not a function.", err.apply); report_eq(err.equation); },
        [&](MismatchedArgType const& err) { report("Mismatched arg type.", err.apply); report_eq(err.equation); },
        [&](BadTypeFamilyType const& err) { report("Bad type family type.", err.type_family); report_eq(err.equation); },
        [&](BadHoleType const& err) { report("Bad hole type.", err.hole); report_eq(err.equation); },
        [&](BadDeclarationType const& err) { report("Bad declaration type.", err.declaration); report_eq(err.equation); },
        [&](BadAxiomType const& err) { report("Bad axiom type.", err.axiom); report_eq(err.equation); },
        [&](BadLetType const& err) { report("Bad let type.", err.let); report_eq(err.equation); },
        [&](BadRequirementType const& err) { report("Bad check type.", err.check); report_eq(err.equation); },
        [&](MismatchedLetType const& err) { report("Mismatched let type.", err.let); report_eq(err.equation); },
        [&](NonmatchableSubclause const& err) { report("Non-matchable subclause head.", err.subclause); },
        [&](MissingCaptureInSubclause const& err) { report("Missing capture in subclause.", err.subclause); },
        [&](MissingCaptureInRule const& err) { report("Missing capture in pattern.", err.rule);  },
        [&](BadApplicationInPattern const& err) { report("Bad application in pattern.", err.rule); },
        [&](BadApplicationInSubclause const& err) { report("Bad application in subclause.", err.subclause); },
        [&](InvalidDoubleCapture const& err) { report_double("Bad double capture.", err.primary_capture, err.secondary_capture); report_eq(err.equation); },
        [&](InvalidNondestructurablePattern const& err) { report("Bad non-destructible match.", err.pattern_part); report_eq(err.equation); },
        [&](MismatchedReplacementType const& err) { report("Mismatched replacement type.", err.rule); report_eq(err.equation); },
        [&](FailedRequirement const& err) { report("Failed requirement.", err.check); report_eq(err.equation); },
        [&](FailedTypeRequirement const& err) { report("Failed type requirement.", err.check); report_eq(err.equation); },
        [&](MismatchedRequirementRHSType const& err) { report("RHS type is not requested type.", err.check); report_eq(err.equation); }
      }, error);
    }
  }
  bool EvaluateInfo::is_okay() const {
    return evaluation_errors.empty();
  }
  void destroy_from_arena(new_expression::Arena& arena, ResolveInfo& resolve_info) {
    destroy_from_arena(arena, resolve_info.embeds);
  }
  void destroy_from_arena(new_expression::Arena& arena, EvaluateInfo& evaluate_info) {
    destroy_from_arena(arena, evaluate_info.result, evaluate_info.evaluation_errors, (ResolveInfo&)evaluate_info);
  }

}
