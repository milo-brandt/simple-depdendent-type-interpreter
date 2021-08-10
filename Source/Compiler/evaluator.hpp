#ifndef COMPILER_EVALUATOR_HPP
#define COMPILER_EVALUATOR_HPP

#include "resolved_tree.hpp"
#include "../Expression/expression_tree.hpp"
#include "../Utility/function.hpp"
#include <unordered_map>

namespace compiler::evaluate {
  struct Cast {
    std::uint64_t depth;
    expression::tree::Tree source_type;
    expression::tree::Tree source;
    expression::tree::Tree target_type;
    expression::tree::Tree target;
  };
  struct Rule {
    expression::tree::Tree pattern;
    expression::tree::Tree replacement;
  };
  namespace variable_explanation {
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
  };
  struct EvaluateResult {
    std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
    std::vector<Cast> casts;
    std::vector<Rule> rules;
    expression::TypedValue result;
  };
  struct EvaluateContext {
    std::uint64_t arrow_axiom;
    std::uint64_t type_axiom;
    mdb::function<expression::TypedValue(std::uint64_t)> embed;
    mdb::function<std::uint64_t()> allocate_variable;
  };
  EvaluateResult evaluate_tree(compiler::resolution::output::Tree const& tree, EvaluateContext& context);
  inline EvaluateResult evaluate_tree(compiler::resolution::output::Tree const& tree, EvaluateContext&& context) {
    return evaluate_tree(tree, context);
  }
}

#endif
