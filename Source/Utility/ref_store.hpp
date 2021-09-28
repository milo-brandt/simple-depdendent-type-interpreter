#ifndef MDB_REF_STORE_HPP
#define MDB_REF_STORE_HPP

#include <type_traits>
#include <utility>

namespace mdb {
  template<class T>
  class ValueStore {
    T value;
  public:
    ValueStore(T&& value):value(std::move(value)) {}
    T&& take() && { return std::move(value); }
    T& peek() { return value; }
  };
  template<class T>
  class RefStore {
    T& value;
  public:
    RefStore(T& value):value(value) {}
    T& take() && { return value; }
    T& peek() { return value; }
  };
  template<class T>
  auto store(std::remove_reference_t<T>& value) {
    if constexpr(std::is_lvalue_reference_v<T>) {
      return RefStore<std::remove_reference_t<T> >{value};
    } else {
      return ValueStore<std::remove_reference_t<T> >{std::move(value)};
    }
  }
  template<class T>
  auto store(std::remove_reference_t<T>&& value) {
    return ValueStore<std::remove_reference_t<T> >{std::move(value)};
  }
}

#endif
