#ifndef NEW_EVALUATION_HPP
#define NEW_EVALUATION_HPP

#include "rule.hpp"
#include <bitset>
#include "../Utility/result.hpp"

namespace new_expression {
  class SimpleEvaluationContext {
    Arena* arena;
    RuleCollector* rule_collector;
  public:
    SimpleEvaluationContext(Arena&, RuleCollector&);
    OwnedExpression reduce_head(OwnedExpression);
  };
  struct EvaluationError{};
  class EvaluationContext {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    EvaluationContext(Arena&, RuleCollector&);
    EvaluationContext(EvaluationContext&&);
    EvaluationContext& operator=(EvaluationContext&&);
    ~EvaluationContext();
    mdb::Result<OwnedExpression, EvaluationError> reduce(OwnedExpression);
    std::optional<EvaluationError> assume_equal(OwnedExpression lhs, OwnedExpression rhs);
    std::optional<EvaluationError> canonicalize_context();
  };
}

#endif
