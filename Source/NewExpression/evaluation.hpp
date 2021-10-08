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
  /*
  class EvaluationContext {
    using Marking = std::bitset<64>;
    struct PointSimplification {
      Marking associated_marks; //must crash if associated_marks & <source marking> has any bits
      std::uint64_t source_arg_count;
      OwnedExpression source;
      OwnedExpression replacement;
    };
    struct ExtraDeclarationInfo {
      std::vector<PointSimplification> extra_simplifications;
    };
    Arena* arena;
    RuleCollector* rule_collector;
    bool needs_canonicalization;
    std::uint64_t next_conglomerate = 0;
    WeakKeyMap<OwnedExpression, PartDestroyer> simplifications;
    WeakKeyMap<OwnedExpression, Marking> markings;
    std::vector<PointSimplification> point_simplifications;
    struct Detail;
  public:
    EvaluationContext(Arena&, RuleCollector&);
    mdb::Result<OwnedExpression, EvaluationError> reduce_head(OwnedExpression);
    std::optional<EvaluationError> assume_equal(OwnedExpression lhs, OwnedExpression rhs);
    std::optional<EvaluationError> canonicalize_context();
  };*/
}

#endif
