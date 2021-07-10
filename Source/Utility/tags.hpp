#ifndef TAGS_HPP
#define TAGS_HPP

namespace mdb {
  struct in_place_t{};
  constexpr in_place_t in_place;
  template<class T> struct in_place_type_t{};
  template<class T> constexpr in_place_type_t<T> in_place_type;
};

#endif
