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
  struct AssumptionClass {
    OwnedExpression representative;
    std::vector<OwnedExpression> terms;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct AssumptionInfo {
    std::vector<AssumptionClass> assumptions;
    static constexpr auto part_info = mdb::parts::simple<1>;
  };
  class EvaluationContext {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    EvaluationContext(Arena&, RuleCollector&);
    EvaluationContext(EvaluationContext const&);
    EvaluationContext(EvaluationContext&&);
    EvaluationContext& operator=(EvaluationContext const&);
    EvaluationContext& operator=(EvaluationContext&&);
    ~EvaluationContext();
    OwnedExpression reduce(OwnedExpression);
    OwnedExpression eliminate_conglomerates(OwnedExpression); //an equivalent expression without conglomerates
    bool is_lambda_like(WeakExpression); //should only be used on things that have already been reduced.
    std::optional<EvaluationError> assume_equal(OwnedExpression lhs, OwnedExpression rhs);
    std::optional<EvaluationError> canonicalize_context();
    AssumptionInfo list_assumptions();
  };
}

#endif
