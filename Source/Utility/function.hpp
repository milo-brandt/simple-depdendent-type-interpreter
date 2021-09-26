#ifndef MDB_FUNCTION_HPP
#define MDB_FUNCTION_HPP

#include <type_traits>
#include <utility>
#include <memory>
#include "tags.hpp"

namespace mdb {
  template<class T>
  class function;
  template<class Ret, class... Args>
  class function<Ret(Args...)> { //move-only function
    struct Impl {
      virtual Ret operator()(Args&&...) = 0;
      virtual ~Impl() = default;
    };
    template<class T>
    struct ImplSpecific : Impl {
      T value;
      template<class... ConstructArgs>
      ImplSpecific(ConstructArgs&&... args):value{std::forward<ConstructArgs>(args)...}{}
      Ret operator()(Args&&... args) { return value(std::forward<Args>(args)...); }
    };
    template<class T>
    static constexpr bool can_call = std::is_invocable_r_v<Ret, T, Args...>;
    std::unique_ptr<Impl> data;
  public:
    function(function&&) = default;
    function(function const&) = delete;
    function& operator=(function&&) = default;
    function& operator=(function const&) = delete;

    template<class T> requires (can_call<T> /*&& std::is_move_constructible_v<T>*/)
    function(T value):data(new ImplSpecific<T>{std::move(value)}) {}
    template<class T, class... ConstructArgs> requires (can_call<T> && ... && std::is_constructible_v<T, ConstructArgs&&>)
    function(in_place_type_t<T>, ConstructArgs&&... args):data(new ImplSpecific<T>{std::forward<ConstructArgs>(args)...}) {}

    Ret operator()(Args... args) const {
      return (*data)(std::forward<Args>(args)...);
    }
  };
}

#endif
