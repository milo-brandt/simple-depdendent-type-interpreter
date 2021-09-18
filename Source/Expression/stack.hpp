#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include "evaluation_context.hpp"

namespace expression {
  class Stack {
    struct Impl;
    std::shared_ptr<Impl> impl;
    Stack(std::shared_ptr<Impl> impl);
  public:
    std::uint64_t depth();
    tree::Expression type_family_type(); // e.g. (x : Nat) -> (y : IsPrime x) -> Type
    tree::Expression instance_of_type_family(Context&, tree::Expression); //Takes F $0 $1 to (x : Nat) -> (y : IsPrime x) -> F x y, for instance.
    tree::Expression type_of_arg(std::uint64_t); //e.g. 1 -> IsPrime $0
    tree::Expression apply_args(tree::Expression); //apply args up to depth
    Stack extend(Context& context, tree::Expression type_family); //e.g. takes F $0 $1 to the context (x : Nat) -> (y : IsPrime x) -> (f : F x y) -> ...
    static Stack empty(Context& context);
  };
}

#endif
