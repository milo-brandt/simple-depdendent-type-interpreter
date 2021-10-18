#ifndef NEW_INSTRUCTION_TREE_EXPLANATION
#define NEW_INSTRUCTION_TREE_EXPLANATION

#include "../ExpressionParser/parser_tree.hpp"

namespace compiler::new_instruction {
  enum class ExplanationKind {
    root, direct_apply,
    pattern_apply, pattern_local, pattern_embed, pattern_hole, pattern_capture,
    lambda_type, lambda_type_local, lambda_type_hole_type, lambda_type_hole, lambda_type_hole_local, lambda_codomain_hole_type, lambda_codomain_hole, lambda_codomain_hole_local, lambda_type_arrow, lambda_type_arrow_apply, lambda_declaration, lambda_declaration_local, lambda_pat_apply, lambda_pat_arg, lambda_pat_head, lambda_rule,
    id_local, id_embed,
    hole_type_type, hole_type, hole_type_local, hole, hole_local,
    arrow_domain, arrow_domain_local, arrow_codomain_type, arrow_codomain_type_family, arrow_codomain, arrow_pattern_head, arrow_pattern_arg, arrow_pattern_apply, arrow_rule, arrow_arrow, arrow_arrow_apply, arrow_arrow_complete, declare, rule_pattern_type_hole_type, rule_pattern_type_hole, rule_pattern_type_local, rule_pattern_arg, rule_pattern_for_all, rule, axiom, let, literal_embed, vector_type_type, vector_type, vector_type_local, vector_element, vector_element_local, vector_empty, vector_empty_typed, vector_push, vector_push_typed, vector_push_vector, vector_push_element
  };
  struct Explanation {
    ExplanationKind kind;
    expression_parser::archive_index::PolymorphicKind index;
  };
  inline bool operator==(Explanation const& lhs, Explanation const& rhs) {
    return lhs.kind == rhs.kind && lhs.index == rhs.index;
  }
  enum class Primitive {
    type, arrow, empty_vec, push_vec
  };
  inline std::ostream& operator<<(std::ostream& o, Primitive primitive) {
    switch(primitive) {
      case Primitive::type: return o << "type";
      case Primitive::arrow: return o << "arrow";
      default: return o << "(invalid primitive)";
    }
  }
}

#endif
