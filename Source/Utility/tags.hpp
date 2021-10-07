#ifndef TAGS_HPP
#define TAGS_HPP

namespace mdb {
  struct Unit{}; //unit type
  constexpr Unit unit{};
  struct in_place_t{};
  constexpr in_place_t in_place;
  template<class T> struct in_place_type_t{};
  template<class T> constexpr in_place_type_t<T> in_place_type;
  template<class T> struct Ref {
    T* ptr;
  };
  template<class T> Ref<T> ref(T& value) { return Ref<T>{&value}; }

  template<class T> struct Tag{};
  template<class T> constexpr Tag<T> tag{};
};

#endif
