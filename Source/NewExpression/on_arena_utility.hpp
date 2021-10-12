#ifndef ON_ARENA_UTILITY_HPP
#define ON_ARENA_UTILITY_HPP

#include "arena.hpp"
#include "../Utility/parts.hpp"

/*
  Destructors and copy operators dealing with arenas
*/

namespace new_expression {
  template<class T>
  void destroy_from_arena(Arena& arena, T& value) {
    if constexpr(std::is_same_v<std::decay_t<T>, OwnedExpression>) {
      arena.drop(std::move(value));
    } else if constexpr(mdb::parts::visitable<T>) {
      mdb::parts::visit_children(value, [&arena](auto& part) {
        destroy_from_arena(arena, part);
      });
    }
  }
  template<class T1, class T2, class... Ts> //nicer way to destroy lots of things at once
  void destroy_from_arena(Arena& arena, T1& v1, T2& v2, Ts&... vn) {
    destroy_from_arena(arena, v1);
    destroy_from_arena(arena, v2);
    (destroy_from_arena(arena, vn) , ...);
  }
  struct PartDestroyer {
    template<class T>
    void operator()(Arena& arena, T& value) {
      destroy_from_arena(arena, value);
    }
  };
}

#endif