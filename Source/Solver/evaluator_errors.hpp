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
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct MismatchedArgType {
    //reported when apply.rhs doesn't have the correct type
    archive_index::Apply apply;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadTypeFamilyType {
    archive_index::TypeFamilyOver type_family;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadHoleType {
    archive_index::DeclareHole hole;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadDeclarationType {
    archive_index::Declare declaration;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadAxiomType {
    archive_index::Axiom axiom;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadLetType {
    archive_index::Let let;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct MismatchedLetType {
    archive_index::Let let;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct NonmatchableSubclause {
    archive_index::Rule rule;
    archive_index::Expression subclause;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct MissingCaptureInSubclause {
    archive_index::Rule rule;
    archive_index::Expression subclause;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadApplicationInPattern {
    archive_index::Rule rule;
    static constexpr auto part_info = mdb::parts::simple<1>;
  };
  struct BadApplicationInSubclause {
    archive_index::Rule rule;
    archive_index::Pattern subclause;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct MissingCaptureInRule {
    archive_index::Rule rule;
    static constexpr auto part_info = mdb::parts::simple<1>;
  };
  struct InvalidDoubleCapture {
    archive_index::Rule rule;
    archive_index::PatternCapture primary_capture;
    archive_index::PatternCapture secondary_capture;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<4>;
  };
  struct InvalidNondestructurablePattern {
    archive_index::Rule rule;
    archive_index::Pattern pattern_part;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct MismatchedReplacementType {
    archive_index::Rule rule;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct BadRequirementType {
    archive_index::Check check;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct FailedRequirement {
    bool allow_deduction;
    archive_index::Check check;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct FailedTypeRequirement {
    bool allow_deduction;
    archive_index::Check check;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct MismatchedRequirementRHSType {
    //reported for instances of "require x : T = y" where y doesn't have type T
    bool allow_deduction;
    archive_index::Check check;
    EquationErrorInfo equation;
    static constexpr auto part_info = mdb::parts::simple<3>;
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
    NonmatchableSubclause,
    MissingCaptureInSubclause,
    MissingCaptureInRule,
    BadApplicationInPattern,
    BadApplicationInSubclause,
    InvalidDoubleCapture,
    InvalidNondestructurablePattern,
    MismatchedReplacementType,
    MismatchedRequirementRHSType,
    BadRequirementType,
    FailedRequirement,
    FailedTypeRequirement
  >;
}

#endif
