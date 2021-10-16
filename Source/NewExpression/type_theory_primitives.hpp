#ifndef TYPE_THEORY_PRIMITIVES_HPP
#define TYPE_THEORY_PRIMITIVES_HPP

#include "arena.hpp"
#include "evaluation.hpp"

namespace new_expression {
  struct TypeTheoryPrimitives {
    OwnedExpression type;
    OwnedExpression arrow;
    OwnedExpression type_family; // \T:Type.T -> Type;
    OwnedExpression constant; // \T:Type.\t:T.Type;
    OwnedExpression constant_codomain_1;
    OwnedExpression constant_codomain_2;
    OwnedExpression constant_codomain_3;
    OwnedExpression constant_codomain_4;
    OwnedExpression id; // \T:Type.\t:T.\S:Type.\s:S.t
    OwnedExpression id_codomain;
    OwnedExpression arrow_codomain;
    OwnedExpression arrow_type;
    std::array<std::pair<WeakExpression, OwnedExpression>, 11> get_types_of_primitives(Arena& arena);
    TypeTheoryPrimitives(Arena& arena, RuleCollector& rules);
    static constexpr auto part_info = mdb::parts::simple<12>;
  };
}

#endif
