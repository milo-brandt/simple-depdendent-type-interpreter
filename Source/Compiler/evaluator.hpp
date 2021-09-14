#ifndef COMPILER_EVALUATOR_HPP
#define COMPILER_EVALUATOR_HPP

#include "instructions.hpp"
#include "../Expression/expression_tree.hpp"
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
  /*namespace variable_explanation {
    struct ExplicitHoleValue {
      resolution::path::Path source;
    };
    struct ExplicitHoleType {
      resolution::path::Path source;
    };
    struct LambdaDeclaration {
      resolution::path::Path source;
    };
    struct LambdaCodomain {
      resolution::path::Path source;
    };
    struct LambdaCastBody {
      resolution::path::Path source;
    };
    struct LambdaCastDomain {
      resolution::path::Path source;
    };
    struct ApplyCastLHS {
      resolution::path::Path source;
    };
    struct ApplyCastRHS {
      resolution::path::Path source;
    };
    struct ApplyDomain {
      resolution::path::Path source;
    };
    struct ApplyCodomain {
      resolution::path::Path source;
    };
    using Any = std::variant<ExplicitHoleValue, ExplicitHoleType, LambdaDeclaration, LambdaCodomain, LambdaCastBody, LambdaCastDomain, ApplyCastLHS, ApplyCastRHS, ApplyDomain, ApplyCodomain>;
  };*/
  struct EvaluateResult {
    //std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
    std::vector<std::uint64_t> variables;
    std::vector<Cast> casts;
    std::vector<Rule> rules;
    expression::TypedValue result;
  };
  struct EvaluateContext {
    expression::TypedValue arrow_axiom;
    expression::TypedValue type_axiom;
    mdb::function<expression::TypedValue(expression::tree::Expression)> type_family_over;
    mdb::function<expression::TypedValue(std::uint64_t)> embed;
    mdb::function<std::uint64_t(bool)> allocate_variable;
  };
  EvaluateResult evaluate_tree(instruction::output::archive_part::ProgramRoot const& tree, EvaluateContext& context);
  inline EvaluateResult evaluate_tree(instruction::output::archive_part::ProgramRoot const& tree, EvaluateContext&& context) {
    return evaluate_tree(tree, context);
  }
}

#endif
