#ifndef COMPILER_EVALUATOR_ERRORS_HPP
#define COMPILER_EVALUATOR_ERRORS_HPP

#include "../Compiler/new_instructions.hpp"
#include "interface_types.hpp"

namespace solver::evaluator::error {
  namespace archive_index = compiler::new_instruction::archive_index;
  struct NotAFunction {
    //reported when apply.lhs is not a function.
    archive_index::Apply apply;
    EquationErrorInfo equation;
  };
  struct MismatchedArgType {
    //reported when apply.rhs doesn't have the correct type
    archive_index::Apply apply;
    EquationErrorInfo equation;
  };
  struct BadTypeFamilyType {
    archive_index::TypeFamilyOver type_family;
    EquationErrorInfo equation;
  };
  struct BadHoleType {
    archive_index::DeclareHole hole;
    EquationErrorInfo equation;
  };
  struct BadDeclarationType {
    archive_index::Declare declaration;
    EquationErrorInfo equation;
  };
  struct BadAxiomType {
    archive_index::Axiom axiom;
    EquationErrorInfo equation;
  };
  struct BadLetType {
    archive_index::Let let;
    EquationErrorInfo equation;
  };
  struct MismatchedLetType {
    archive_index::Let let;
    EquationErrorInfo equation;
  };
  struct MissingCaptureInSubclause {
    archive_index::Rule rule;
    archive_index::Expression subclause;
  };
  struct MissingCaptureInRule {
    archive_index::Rule rule;
  };
  struct InvalidDoubleCapture {
    archive_index::Rule rule;
    archive_index::PatternCapture primary_capture;
    archive_index::PatternCapture secondary_capture;
    EquationErrorInfo equation;
  };
  struct InvalidNondestructurablePattern {
    archive_index::Rule rule;
    archive_index::Pattern pattern_part;
    EquationErrorInfo equation;
  };
  struct MismatchedReplacementType {
    archive_index::Rule rule;
    EquationErrorInfo equation;
  };
  using Any = std::variant<
    NotAFunction,
    MismatchedArgType,
    BadTypeFamilyType,
    BadHoleType,
    BadDeclarationType,
    BadAxiomType,
    BadLetType,
    MismatchedLetType,
    MissingCaptureInSubclause,
    MissingCaptureInRule,
    InvalidDoubleCapture,
    InvalidNondestructurablePattern,
    MismatchedReplacementType
  >;
}

#endif
