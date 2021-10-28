#ifndef TYPE_UTILITY_HPP
#define TYPE_UTILITY_HPP

#include "arena.hpp"
#include "arena_container.hpp"
#include "on_arena_utility.hpp"

namespace new_expression {
  struct FunctionInfo {
    WeakExpression domain;
    WeakExpression codomain;
  };
  std::optional<FunctionInfo> get_function_data(Arena& arena, WeakExpression target, WeakExpression arrow);
  struct TypeGetterContext {
    WeakExpression arrow;
    mdb::function<OwnedExpression(WeakExpression)> type_of_primitive;
    mdb::function<OwnedExpression(OwnedExpression)> reduce_head;
  };
  OwnedExpression get_type_of(Arena& arena, WeakExpression, TypeGetterContext const&);
  struct PrimitiveTypeCollector {
    WeakKeyMap<OwnedExpression, PartDestroyer> type_of_primitive;
    void set_type_of(WeakExpression primitive, OwnedExpression type) { type_of_primitive.set(primitive, std::move(type)); }
    WeakExpression get_type_of(WeakExpression primitive) { return type_of_primitive.at(primitive); }
    bool has_type(WeakExpression primitive) { return type_of_primitive.contains(primitive); }
  };
}

#endif
