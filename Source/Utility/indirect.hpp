#ifndef INDIRECT_HPP
#define INDIRECT_HPP

#include <memory>
#include <type_traits>
#include "tags.hpp"

namespace mdb {
  template<class T>
  class Indirect {
    std::unique_ptr<T> data;
  public:
    template<class... Args> requires std::is_constructible_v<T, Args&&...>
    explicit Indirect(mdb::in_place_t, Args&&... args):data(new T{std::forward<Args>(args)...}) {}
    Indirect(Indirect const& other):data(other.data ? new T{*other.data}: nullptr) {}
    Indirect(Indirect&& other) = default;
    Indirect& operator=(Indirect const& other) {
      if(other.data) {
        if(data) {
          *data = *other.data;
        } else {
          data.reset(new T{*other.data});
        }
      } else {
        data.reset();
      }
      return *this;
    }
    Indirect& operator=(Indirect&& other) {
      data = std::move(other.data);
      return *this;
    }
    T& operator*() {
      return *data;
    }
    T const& operator*() const {
      return *data;
    }
    T* operator->() {
      return data.get();
    }
    T const* operator->() const {
      return data.get();
    }
    T* get() {
      return data.get();
    }
    T const* get() const {
      return data.get();
    }
  };
}


#endif
