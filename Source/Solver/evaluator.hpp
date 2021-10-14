#ifndef COMPILER_EVALUATOR_HPP
#define COMPILER_EVALUATOR_HPP

#include "../Compiler/instructions.hpp"
#include "stack.hpp"
#include "../NewExpression/evaluation.hpp"
#include "../NewExpression/typed_value.hpp"
#include "interface_types.hpp"

namespace solver::evaluator {
  struct EvaluatorInterface {
    new_expression::Arena& arena;
    new_expression::WeakExpression type;
    new_expression::WeakExpression arrow;
    new_expression::WeakExpression arrow_type;
    new_expression::WeakExpression type_family;
    new_expression::WeakExpression id;
    std::function<void(new_expression::WeakExpression)> register_declaration;
    std::function<void(new_expression::Rule)> add_rule;
    mdb::function<new_expression::OwnedExpression(new_expression::OwnedExpression)> reduce;
    mdb::function<void(Cast)> cast;
    mdb::function<void(FunctionCast)> function_cast;
    mdb::function<void(Rule)> rule;
    mdb::function<new_expression::TypedValue(std::uint64_t)> embed;
  };
  new_expression::TypedValue evaluate(compiler::instruction::output::archive_part::ProgramRoot const& tree, EvaluatorInterface interface);
  /**/
  /*struct RuleExplanation {
    instruction::archive_index::Rule index;
  };
  namespace variable_explanation {
    namespace archive_index = instruction::archive_index;
    struct ApplyRHSCast {
      std::uint64_t depth;
      archive_index::Apply index;
    };
    struct ApplyDomain {
      std::uint64_t depth;
      archive_index::Apply index;
    };
    struct ApplyCodomain {
      std::uint64_t depth;
      archive_index::Apply index;
    };
    struct ApplyLHSCast {
      std::uint64_t depth;
      archive_index::Apply index;
    };
    struct ExplicitHole {
      std::uint64_t depth;
      archive_index::DeclareHole index;
    };
    struct Declaration {
      std::uint64_t depth;
      archive_index::Declare index;
    };
    struct Axiom {
      std::uint64_t depth;
      archive_index::Axiom index;
    };
    struct TypeFamilyCast {
      std::uint64_t depth;
      archive_index::TypeFamilyOver index;
    };
    struct HoleTypeCast {
      std::uint64_t depth;
      archive_index::DeclareHole index;
    };
    struct DeclareTypeCast {
      std::uint64_t depth;
      archive_index::Declare index;
    };
    struct AxiomTypeCast {
      std::uint64_t depth;
      archive_index::Axiom index;
    };
    struct LetTypeCast {
      std::uint64_t depth;
      archive_index::Let index;
    };
    struct LetCast {
      std::uint64_t depth;
      archive_index::Let index;
    };
    struct ForAllTypeCast {
      std::uint64_t depth;
      archive_index::ForAll index;
    };
    struct VarType {
      std::uint64_t depth;
      std::uint64_t base_variable;
      archive_index::PolymorphicKind index;
    };
    using Any = std::variant<ApplyRHSCast, ApplyDomain, ApplyCodomain, ApplyLHSCast, ExplicitHole, Declaration, Axiom, TypeFamilyCast, HoleTypeCast, DeclareTypeCast, AxiomTypeCast, LetCast, LetTypeCast, ForAllTypeCast, VarType>;
    inline bool is_axiom(Any const& any) { return std::holds_alternative<Axiom>(any); }
    inline bool is_declaration(Any const& any) { return std::holds_alternative<Declaration>(any); }
    inline bool is_variable(Any const& any) { return std::holds_alternative<ApplyDomain>(any) || std::holds_alternative<ApplyCodomain>(any) || std::holds_alternative<ExplicitHole>(any); }
    inline bool is_indeterminate(Any const& any) { return !is_axiom(any) && !is_declaration(any); }
  };
  namespace pattern_variable_explanation {
    namespace archive_index = pattern::archive_index;
    struct ApplyCast { //always casts argument, never function
      std::uint64_t arg_index;
      archive_index::Segment index;
    };
    struct CapturePoint {
      archive_index::CapturePoint index;
    };
    struct CapturePointType {
      archive_index::CapturePoint index;
    };
    using Any = std::variant<ApplyCast, CapturePoint, CapturePointType>;
  };*/
  /*struct EvaluateResult {
    std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
    std::vector<Cast> casts;
    std::vector<FunctionCast> function_casts;
    std::vector<Rule> rules;
    std::vector<RuleExplanation> rule_explanations;
    expression::TypedValue result;
    compiler::instruction::forward_locator::archive_root::Program forward_locator;
  };
  EvaluateResult evaluate_tree(instruction::output::archive_part::ProgramRoot const& tree, expression::Context& expression_context, mdb::function<expression::TypedValue(std::uint64_t)> embed);*/
}

#endif
