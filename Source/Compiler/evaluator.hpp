#ifndef COMPILER_EVALUATOR_HPP
#define COMPILER_EVALUATOR_HPP

#include "instructions.hpp"
#include "../Expression/evaluation_context.hpp"
#include "../Utility/function.hpp"
#include <unordered_map>

namespace compiler::evaluate {
  struct Cast {
    std::uint64_t depth;
    std::uint64_t variable;
    expression::tree::Expression source_type;
    expression::tree::Expression source;
    expression::tree::Expression target_type;
  };
  struct Rule {
    std::uint64_t depth;
    expression::tree::Expression pattern_type;
    expression::tree::Expression pattern;
    expression::tree::Expression replacement_type;
    expression::tree::Expression replacement;
  };
  namespace variable_explanation {
    namespace archive_index = instruction::archive_index;
    struct ApplyRHSCast {
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
    using Any = std::variant<ApplyRHSCast, ApplyCodomain, ApplyLHSCast, ExplicitHole, Declaration, Axiom, TypeFamilyCast, HoleTypeCast, DeclareTypeCast, AxiomTypeCast, LetCast, LetTypeCast, ForAllTypeCast, VarType>;
    inline bool is_axiom(Any const& any) { return std::holds_alternative<Axiom>(any); }
    inline bool is_declaration(Any const& any) { return std::holds_alternative<Declaration>(any); }
    inline bool is_variable(Any const& any) { return std::holds_alternative<ApplyCodomain>(any) || std::holds_alternative<ExplicitHole>(any); }
    inline bool is_indeterminate(Any const& any) { return !is_axiom(any) && !is_declaration(any); }
  };
  struct EvaluateResult {
    std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
    std::vector<Cast> casts;
    std::vector<Rule> rules;
    expression::TypedValue result;
  };
  EvaluateResult evaluate_tree(instruction::output::archive_part::ProgramRoot const& tree, expression::Context& expression_context, mdb::function<expression::TypedValue(std::uint64_t)> embed);
}

#endif
