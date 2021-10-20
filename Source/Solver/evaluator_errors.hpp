#ifndef COMPILER_EVALUATOR_ERRORS_HPP
#define COMPILER_EVALUATOR_ERRORS_HPP

#include "../Compiler/new_instructions.hpp"

namespace solver::evaluator::error {
  struct EquationErrorInfo{};
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
    archive_index::Let declaration;
    EquationErrorInfo equation;
  };

}

#endif
