#include <variant>
#include "lifted_state_machine.hpp"

template<class T, class E>
struct result {
  std::variant<T, E> as_variant;
  bool operator==(result<T,E> const&) const = default;
  static result success(T value) {
    return result{{std::in_place_index<0>, std::move(value)}};
  }
  static result failure(E value) {
    return result{{std::in_place_index<1>, std::move(value)}};
  }
};
template<class T, class E>
struct result_routine_definition;
template<class T, class E>
using result_routine = lifted_state_machine::coro_type<result_routine_definition<T, E> >;
template<class T, class E>
struct result_routine_definition : lifted_state_machine::coro_base<result_routine_definition<T, E> > {
  using coro_base =  lifted_state_machine::coro_base<result_routine_definition<T, E> >;
  template<class V = void>
  using resume_handle = typename coro_base::template resume_handle<V>;
  struct state {};
  using paused_type = result<T, E>;
  class starter_base {
    resume_handle<> handle;
    starter_base(resume_handle<> handle):handle(std::move(handle)){}
    friend result_routine_definition;
  public:
    result<T, E> get() && { return std::move(handle).resume(); }
    operator result<T, E>() && { return std::move(*this).get(); }
  };
  static starter_base create_object(resume_handle<> handle) {
    return std::move(handle);
  }
  template<class V>
  static coro::variant_awaiter<V, lifted_state_machine::immediate_awaiter<V>, lifted_state_machine::await_with_no_resume<V> >
    on_await(result<V, E> r, state&, typename coro_base::waiting_handle&& handle) {
    if(r.as_variant.index() == 1) return std::move(handle).template destroying_result<V>({std::variant<T,E>{std::in_place_index<1>, std::get<1>(std::move(r.as_variant))}});
    else return std::move(handle).immediate_result(std::move(std::get<0>(r.as_variant)));
  }
  template<class V>
  static auto on_await(result_routine<V, E> r, state& s, typename coro_base::waiting_handle&& handle) {
    return on_await(std::move(r).get(), s, std::move(handle));
  }
  static auto on_yield(E error, state&, typename coro_base::waiting_handle&& handle) {
    return std::move(handle).destroying_result({std::variant<T,E>{std::in_place_index<1>, std::move(error)}});
  }
  static void on_return_value(T value, state&, typename coro_base::returning_handle&& handle) {
    return std::move(handle).result({std::variant<T,E>{std::in_place_index<0>, std::move(value)}});
  }
};
