#ifndef EXPRESSION_ARENA_CONTAINER_HPP
#define EXPRESSION_ARENA_CONTAINER_HPP

#include "arena.hpp"
#include <unordered_map>

/*
  This file defines utilities for holding expressions.

  It is largely necessitated by the fact that it's difficult to
  make a good RAII container for expressions, since the arena
  to which they belong needs to be notified on destruction.
*/
namespace new_expression {
  struct ExpressionHasher {
    std::hash<std::size_t> hash;
    auto operator()(WeakExpression const& expr) const noexcept { return hash(expr.index()); }
    auto operator()(UniqueExpression const& expr) const noexcept { return hash(expr.get().index()); }
    auto operator()(OwnedExpression const& expr) const noexcept { return hash(expr.index()); }
  };
  template<class T>
  class WeakKeyMap {
    Arena* arena;
    std::unordered_map<WeakExpression, T, ExpressionHasher> underlying_map;
    mdb::Event<std::span<WeakExpression> >::UniqueHook listener;
    static auto listener_for_arena(WeakKeyMap* me, Arena& arena) {
      return arena.terms_erased().listen_unique([me](std::span<WeakExpression> span) {
        for(auto expr : span) {
          if(me->underlying_map.contains(expr)) {
            me->underlying_map.erase(expr);
          }
        }
      });
    }
  public:
    WeakKeyMap(Arena& arena):arena(&arena), listener(listener_for_arena(this, arena)) {}
    WeakKeyMap(WeakKeyMap&& other):arena(other.arena), underlying_map(std::move(other.underlying_map)), listener(listener_for_arena(this, *other.arena)) {
      std::move(other.listener).disconnect();
    }
    WeakKeyMap& operator=(WeakKeyMap&& other) {
      arena = other.arena;
      underlying_map = std::move(other.underlying_map);
      std::move(other.listener).disconnect();
      listener = listener_for_arena(this, arena);
      return *this;
    }
    bool contains(WeakExpression key) const { return underlying_map.contains(key); }
    T& at(WeakExpression key) { return underlying_map.at(key); }
    T const& at(WeakExpression key) const { return underlying_map.at(key); }
    void erase(WeakExpression key) { underlying_map.erase(key); }
    std::size_t size() const { return underlying_map.size(); }
    void set(WeakExpression key, T value) { underlying_map.insert_or_assign(key, std::move(value)); }
  };
}


#endif
