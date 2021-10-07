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
  struct PartDestroyer {
    template<class T>
    void operator()(Arena& arena, T& value) {
      destroy_from_arena(arena, value);
    }
  };
}

#endif
