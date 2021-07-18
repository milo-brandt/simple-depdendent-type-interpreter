#include "../ExpressionParser/parser_tree.hpp"
#include <variant>

namespace compiler::resolution::relative {
  struct Root{};
  struct Step {
    std::uint64_t step;
  };
  struct HoleExpandApply {
    std::uint64_t arg_index;
  };
  struct HoleExpandArg {
    std::uint64_t arg_index;
  };
  struct LambdaType {};
  enum class ArrowExpand {
    arrow_axiom,
    apply_domain,
    codomain_lambda
  };
  using Position = std::variant<Root, Step, HoleExpandApply, HoleExpandArg, ArrowExpand, LambdaType>;
  inline bool operator==(Root const&, Root const&) { return true; }
  inline bool operator==(Step const& lhs, Step const& rhs) { return lhs.step == rhs.step; }
  inline bool operator==(HoleExpandApply const& lhs, HoleExpandApply const& rhs) { return lhs.arg_index == rhs.arg_index; }
  inline bool operator==(HoleExpandArg const& lhs, HoleExpandArg const& rhs) { return lhs.arg_index == rhs.arg_index; }
  inline bool operator==(LambdaType const& lhs, LambdaType const& rhs) { return true; }
};
