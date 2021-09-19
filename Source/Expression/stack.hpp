#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include "evaluation_context.hpp"

namespace expression {
  class Stack {
    struct Impl;
    std::shared_ptr<Impl> impl;
    Stack(std::shared_ptr<Impl> impl);
  public:
    std::uint64_t depth() const;
    tree::Expression type_family_type() const; // e.g. (x : Nat) -> (y : IsPrime x) -> Type
    tree::Expression instance_of_type_family(Context&, tree::Expression) const; //Takes F $0 $1 to (x : Nat) -> (y : IsPrime x) -> F x y, for instance.
    tree::Expression type_of_arg(std::uint64_t) const; //e.g. 1 returns IsPrime $0
    tree::Expression apply_args(tree::Expression) const; //apply args up to depth
    tree::Expression type_of(Context& context, tree::Expression expression) const;
    Stack extend(Context& context, tree::Expression type_family) const; //e.g. takes F $0 $1 to the context (x : Nat) -> (y : IsPrime x) -> (f : F x y) -> ...
    static Stack empty(Context& context);
  };
}

#endif
