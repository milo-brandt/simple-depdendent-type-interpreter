#ifndef NEW_STACK_HPP
#define NEW_STACK_HPP

#include "../NewExpression/evaluation.hpp"
#include "../NewExpression/typed_value.hpp"
#include "../NewExpression/type_utility.hpp"
#include <functional>

namespace stack {
  struct StackInterface {
    new_expression::WeakExpression type; //Type
    new_expression::WeakExpression arrow; //\S:Type.\T:S -> Type.(x : S) -> T x;
    new_expression::WeakExpression id; //\T:Type.\x:T.x
    new_expression::WeakExpression constant; //\T:Type.\x:T.\S:Type.\y:S.x
    new_expression::WeakExpression type_family; //\T:Type.T -> Type
    new_expression::Arena& arena;
    new_expression::RuleCollector& rule_collector;
    new_expression::PrimitiveTypeCollector& type_collector;
    std::function<void(new_expression::WeakExpression, new_expression::OwnedExpression)> register_type;
    std::function<void(new_expression::WeakExpression)> register_declaration;
    std::function<void(new_expression::Rule)> add_rule;
  };
  class Stack {
    struct Impl;
    std::shared_ptr<Impl> impl;
    Stack(std::shared_ptr<Impl> impl);
  public:
    std::uint64_t depth() const;
    new_expression::WeakExpression type_family_type() const; // e.g. (x : Nat) -> (y : IsPrime x) -> Type
    new_expression::OwnedExpression instance_of_type_family(new_expression::OwnedExpression) const; //Takes F $0 $1 to (x : Nat) -> (y : IsPrime x) -> F x y, for instance.
    new_expression::OwnedExpression type_of_arg(std::uint64_t) const; //e.g. 1 returns IsPrime $0
    new_expression::OwnedExpression type_of(new_expression::WeakExpression) const;
    new_expression::OwnedExpression apply_args(new_expression::OwnedExpression) const; //apply args up to depth
    new_expression::OwnedExpression reduce(new_expression::OwnedExpression) const;
    new_expression::OwnedExpression eliminate_conglomerates(new_expression::OwnedExpression) const;
    Stack extend_by_assumption(new_expression::TypedValue, new_expression::TypedValue) const;
    Stack extend(new_expression::OwnedExpression type_family) const; //e.g. takes F $0 $1 to the context (x : Nat) -> (y : IsPrime x) -> (f : F x y) -> ...
    static Stack empty(StackInterface context);
  };
}

#endif
