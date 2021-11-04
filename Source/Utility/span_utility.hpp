#ifndef MDB_SPAN_UTILITY_HPP
#define MDB_SPAN_UTILITY_HPP

#include <array>
#include <span>
#include <type_traits>

namespace mdb {
  template<std::size_t n, class T, class Map, class R = std::invoke_result_t<Map, T const&> >
  std::array<R, n> map_span(T* start, T* end, Map&& map) {
    if(end - start != n) std::terminate();
    return [&]<std::size_t... is>(std::index_sequence<is...>) {
      return std::array<R, n>{
        map(start[is])...
      };
    }(std::make_index_sequence<n>{});
  }
  template<std::size_t n, class T, class Map, class R = std::invoke_result_t<Map, T const&> >
  std::array<R, n> map_span(std::span<T> span, Map&& map) {
    return destructure_span<n>(span.begin(), span.end(), std::forward<Map>(map));
  }
}

#endif
