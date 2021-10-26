#ifndef EXPRESSION_ARENA_CONTAINER_HPP
#define EXPRESSION_ARENA_CONTAINER_HPP

#include "arena.hpp"
#include <unordered_map>
#include <unordered_set>

/*
  This file defines utilities for holding expressions.

  It is largely necessitated by the fact that it's difficult to
  make a good RAII container for expressions, since the arena
  to which they belong needs to be notified on destruction.
*/
namespace new_expression {
  struct ExpressionHasher {
    std::hash<void const*> hash;
    auto operator()(WeakExpression const& expr) const noexcept { return hash(expr.data()); }
    auto operator()(OwnedExpression const& expr) const noexcept { return hash(expr.data()); }
    using is_transparent = int;
  };
  struct ExpressionComparer {
    template<class S, class T>
    bool operator()(S const& lhs, T const& rhs) const { return lhs.index() == rhs.index(); }
    using is_transparent = int;
  };
  struct TrivialOnArenaDestructor {
    template<class T>
    void operator()(Arena&, T&&) {}
  };
  template<class T, class OnArenaDestructor = TrivialOnArenaDestructor>
  class WeakKeyMap {
    Arena* arena;
    OnArenaDestructor on_arena_destructor;
    std::unordered_map<WeakExpression, T, ExpressionHasher> underlying_map;
    mdb::Event<std::span<WeakExpression> >::UniqueHook listener;
    static auto listener_for_arena(WeakKeyMap* me, Arena& arena) {
      return arena.terms_erased().listen_unique([me](std::span<WeakExpression> span) {
        for(auto expr : span) {
          if(me->underlying_map.contains(expr)) {
            if constexpr(!std::is_same_v<OnArenaDestructor, TrivialOnArenaDestructor>) {
              me->on_arena_destructor(*me->arena, me->underlying_map.at(expr));
            }
            me->underlying_map.erase(expr);
          }
        }
      });
    }
    void clear_map() {
      if constexpr(!std::is_same_v<OnArenaDestructor, TrivialOnArenaDestructor>) {
        for(auto& term : underlying_map) {
          on_arena_destructor(*arena, term.second);
        }
      }
      underlying_map.clear();
    }
  public:
    WeakKeyMap(Arena& arena):arena(&arena), listener(listener_for_arena(this, arena)) {}
    WeakKeyMap(Arena& arena, OnArenaDestructor on_arena_destructor):arena(&arena), on_arena_destructor(std::move(on_arena_destructor)), listener(listener_for_arena(this, arena)){}

    WeakKeyMap(WeakKeyMap&& other):arena(other.arena), underlying_map(std::move(other.underlying_map)), listener(listener_for_arena(this, *other.arena)) {
      std::move(other.listener).disconnect();
      other.underlying_map.clear(); //just in case the move constructor is defined weirdly
    }
    WeakKeyMap& operator=(WeakKeyMap&& other) {
      if(&other == this) return *this;
      clear_map();
      on_arena_destructor = std::move(other.on_arena_destructor);
      arena = other.arena;
      underlying_map = std::move(other.underlying_map);
      other.underlying_map.clear();
      std::move(other.listener).disconnect();
      listener = listener_for_arena(this, arena);
      return *this;
    }
    bool contains(WeakExpression key) const { return underlying_map.contains(key); }
    T& at(WeakExpression key) { return underlying_map.at(key); }
    T const& at(WeakExpression key) const { return underlying_map.at(key); }
    void erase(WeakExpression key) {
      if(underlying_map.contains(key)) {
        on_arena_destructor(*arena, underlying_map.at(key));
        underlying_map.erase(key);
      }
    }
    std::size_t size() const { return underlying_map.size(); }
    void set(WeakExpression key, T value) {
      if constexpr(!std::is_same_v<OnArenaDestructor, TrivialOnArenaDestructor>) {
        if(underlying_map.contains(key)) {
          auto& entry = underlying_map.at(key);
          on_arena_destructor(*arena, entry);
          entry = std::move(value);
        } else {
          underlying_map.insert(std::make_pair(key, std::move(value)));
        }
      } else {
        underlying_map.insert_or_assign(key, std::move(value));
      }
    }
    ~WeakKeyMap() {
      clear_map();
    }
  };
  template<class T, class OnArenaDestructor = TrivialOnArenaDestructor>
  class OwnedKeyMap {
    Arena* arena;
    OnArenaDestructor on_arena_destructor;
    std::unordered_map<WeakExpression, T, ExpressionHasher> underlying_map;
    void clear_map() {
      for(auto& term : underlying_map) {
        arena->deref_weak(term.first);
        on_arena_destructor(*arena, term.second);
      }
      underlying_map.clear();
    }
  public:
    OwnedKeyMap(Arena& arena):arena(&arena) {}
    OwnedKeyMap(Arena& arena, OnArenaDestructor on_arena_destructor):arena(&arena), on_arena_destructor(std::move(on_arena_destructor)) {}

    OwnedKeyMap(OwnedKeyMap&& other):arena(other.arena), underlying_map(std::move(other.underlying_map)) {
      other.underlying_map.clear(); //just in case the move constructor is defined weirdly
    }
    OwnedKeyMap& operator=(OwnedKeyMap&& other) {
      if(&other == this) return *this;
      clear_map();
      on_arena_destructor = std::move(other.on_arena_destructor);
      arena = other.arena;
      underlying_map = std::move(other.underlying_map);
      other.underlying_map.clear();
      return *this;
    }
    bool contains(WeakExpression key) const { return underlying_map.contains(key); }
    T& at(WeakExpression key) { return underlying_map.at(key); }
    T const& at(WeakExpression key) const { return underlying_map.at(key); }
    void erase(WeakExpression key) {
      if(underlying_map.contains(key)) {
        arena->deref_weak(key);
        on_arena_destructor(*arena, underlying_map.at(key));
        underlying_map.erase(key);
      }
    }
    std::size_t size() const { return underlying_map.size(); }
    void set(OwnedExpression key, T value) {
      if(underlying_map.contains(key)) {
        auto& entry = underlying_map.at(key);
        on_arena_destructor(*arena, entry);
        entry = std::move(value);
        arena->drop(std::move(key));
      } else {
        underlying_map.insert(std::make_pair(std::move(key), std::move(value)));
        //keep extra reference to key
      }
    }
    ~OwnedKeyMap() {
      clear_map();
    }
  };
  class OwnedKeySet {
    Arena* arena;
    std::unordered_set<WeakExpression, ExpressionHasher> inner;
  public:
    OwnedKeySet(Arena& arena):arena(&arena) {}
    OwnedKeySet(OwnedKeySet&& o):arena(o.arena),inner(std::move(o.inner)) { o.inner.clear(); }
    OwnedKeySet& operator=(OwnedKeySet&& o) {
      if(this == &o) return *this;
      arena = o.arena;
      inner = std::move(o.inner);
      o.inner.clear();
      return *this;
    }
    bool insert(OwnedExpression expr) {
      WeakExpression weak = expr;
      if(inner.contains(weak)) {
        arena->drop(std::move(expr));
        return false;
      } else {
        inner.insert(weak);
        //maintain reference expr (until erased)
        return true;
      }
    }
    bool contains(WeakExpression expr) {
      return inner.contains(expr);
    }
    bool erase(WeakExpression expr) {
      if(inner.contains(expr)) {
        arena->deref_weak(expr);
        inner.erase(expr);
        return true;
      } else {
        return false;
      }
    }
    ~OwnedKeySet() {
      for(auto key : inner) {
        arena->deref_weak(key);
      }
    }
  };
}


#endif
