#ifndef COMPILER_EVALUATOR_HPP
#define COMPILER_EVALUATOR_HPP

#include "../Compiler/new_instructions.hpp"
#include "stack.hpp"
#include "../NewExpression/evaluation.hpp"
#include "../NewExpression/typed_value.hpp"
#include "../NewExpression/type_utility.hpp"
#include "interface_types.hpp"
#include "evaluator_errors.hpp"

namespace solver::evaluator {
  namespace variable_explanation {
    namespace archive_index = compiler::new_instruction::archive_index;
    struct ApplyRHSCast {
      archive_index::Apply index;
    };
    struct ApplyDomain {
      archive_index::Apply index;
    };
    struct ApplyCodomain {
      archive_index::Apply index;
    };
    struct ApplyLHSCast {
      archive_index::Apply index;
    };
    struct ExplicitHole {
      archive_index::DeclareHole index;
    };
    struct Declaration {
      archive_index::Declare index;
    };
    struct Axiom {
      archive_index::Axiom index;
    };
    struct TypeFamilyCast {
      archive_index::TypeFamilyOver index;
    };
    struct HoleTypeCast {
      archive_index::DeclareHole index;
    };
    struct DeclareTypeCast {
      archive_index::Declare index;
    };
    struct AxiomTypeCast {
      archive_index::Axiom index;
    };
    struct LetTypeCast {
      archive_index::Let index;
    };
    struct LetCast {
      archive_index::Let index;
    };
    using Any = std::variant<ApplyRHSCast, ApplyDomain, ApplyCodomain, ApplyLHSCast, ExplicitHole, Declaration, Axiom, TypeFamilyCast, HoleTypeCast, DeclareTypeCast, AxiomTypeCast, LetCast, LetTypeCast>;
    inline bool is_axiom(Any const& any) { return std::holds_alternative<Axiom>(any); }
    inline bool is_declaration(Any const& any) { return std::holds_alternative<Declaration>(any); }
    inline bool is_variable(Any const& any) { return std::holds_alternative<ApplyDomain>(any) || std::holds_alternative<ApplyCodomain>(any) || std::holds_alternative<ExplicitHole>(any); }
    inline bool is_indeterminate(Any const& any) { return !is_axiom(any) && !is_declaration(any); }
  };
  struct EvaluatorInterface {
    new_expression::Arena& arena;
    new_expression::RuleCollector& rule_collector;
    new_expression::PrimitiveTypeCollector& type_collector;
    new_expression::WeakExpression type;
    new_expression::WeakExpression arrow;
    new_expression::WeakExpression arrow_type;
    new_expression::WeakExpression type_family;
    new_expression::WeakExpression id;
    new_expression::WeakExpression constant;
    std::function<void(new_expression::WeakExpression, new_expression::OwnedExpression)> register_type;
    std::function<void(new_expression::WeakExpression)> register_declaration;
    std::function<void(new_expression::OwnedExpression, stack::Stack)> register_definable_indeterminate;
    std::function<void(new_expression::Rule)> add_rule;
    std::function<void(new_expression::WeakExpression, variable_explanation::Any)> explain_variable;
    mdb::function<mdb::Future<EquationResult>(Equation)> solve;
    mdb::function<void(error::Any)> report_error;
    mdb::function<new_expression::TypedValue(std::uint64_t)> embed;
    mdb::function<void()> close_interface;
  };
  struct EvaluatorResult {

  };
  new_expression::TypedValue evaluate(compiler::new_instruction::output::archive_part::ProgramRoot const& tree, EvaluatorInterface interface);
}

#endif
