#include "template_utility.hpp"
#include "coro_base.hpp"
#include <variant>
#include <iostream>
/*
Represents coroutines that can co_await, co_yield, or co_return some (finite) set of possibilities and then be resumed given appropriate further input.


*/



namespace lifted_state_machine {
  template<class T = void>
  struct await_from_pointer {
    void** where;
    bool await_ready(){ return false; }
    void await_suspend(std::coroutine_handle<>){}
    T await_resume(){
      T ret = std::move(*(T*)*where);
      return ret;
    }
  };
  template<>
  struct await_from_pointer<void> {
    bool await_ready(){ return false; }
    void await_suspend(std::coroutine_handle<>){}
    void await_resume(){}
  };
  template<class T = void>
  struct immediate_awaiter {
    T value;
    bool await_ready(){ return true; }
    void await_suspend(std::coroutine_handle<>){ std::terminate(); }
    T await_resume() {
      return std::move(value);
    }
  };
  template<>
  struct immediate_awaiter<void> {
    bool await_ready(){ return true; }
    void await_suspend(std::coroutine_handle<>){ std::terminate(); }
    void await_resume(){ }
  };
  template<class T = void>
  struct await_with_no_resume {
    bool await_ready(){ return false; }
    void await_suspend(std::coroutine_handle<>){}
    T await_resume(){ std::terminate(); }
  };
  //awaitable: can accept handle and return useful stuff
  template<class T>
  struct __void_hack{ using type = T; };
  template<>
  struct __void_hack<void>{ using type = std::monostate; };
  template<class T>
  using __void_hack_v = typename __void_hack<T>::type;
  template<class definition_t>
  struct coro_type;
  template<class definition_t, class promise_type, bool x>
  class __coro_base_returning_helper;
  template<class definition_t, class promise_type>
  struct __coro_base_returning_helper<definition_t, promise_type, true> {
    auto return_void() {
      return definition_t::on_return_void(((promise_type*)this)->state, ((promise_type*)this)->get_returning_handle());
    }
  };
  template<class definition_t, class promise_type>
  struct __coro_base_returning_helper<definition_t, promise_type, false> {
    template<class T>
    auto return_value(T&& v) {
      return definition_t::on_return_value(std::forward<T>(v), ((promise_type*)this)->state, ((promise_type*)this)->get_returning_handle());
    }
  };
  template<class definition_t>
  class coro_base {
  protected:
    class waiting_handle;
    class returning_handle;
  public:
    struct promise_type;
    template<class T = void>
    class resume_handle {
      coro::unique_coro_handle<promise_type> handle;
      friend waiting_handle;
      friend promise_type;
      resume_handle(coro::unique_coro_handle<promise_type> handle):handle(std::move(handle)){}
    public:
      resume_handle() = default;
      auto resume() && requires std::is_void_v<T> {
        auto raw = handle.move();
        return raw.promise().run();
      }
      auto resume(__void_hack_v<T> value) && requires (!std::is_void_v<T>) {
        auto raw = handle.move();
        return raw.promise().run(std::move(value));
      }
      auto& state() {
        return handle.promise().state;
      }
    };
    static constexpr bool is_void_return = requires(definition_t d, returning_handle h) {
      definition_t::on_return_void(std::move(h));
    };
    struct promise_type : __coro_base_returning_helper<definition_t, promise_type, is_void_return>  {
      using return_slot_type = std::variant<std::pair<bool, typename definition_t::paused_type>, std::exception_ptr>;
      typename definition_t::state state;
      void* passing_slot;
      auto get_returning_handle() {
        return returning_handle(this);
      }
      template<class T>
      auto await_transform(T&& value) {
        return definition_t::on_await(std::forward<T>(value), state, waiting_handle{this});
      }
      template<class T>
      auto yield_value(T&& value) {
        return definition_t::on_yield(std::forward<T>(value), state, waiting_handle{this});
      }
      void set_return_value(bool done, typename definition_t::paused_type&& v) {
        passing_slot = new return_slot_type(std::make_pair(done, std::move(v)));
      }
      void set_return_exception(std::exception_ptr e) {
        passing_slot = new return_slot_type(std::move(e));
      }
      auto get_coro_handle() {
        return std::coroutine_handle<promise_type>::from_promise(*this);
      }
      typename definition_t::paused_type get_return_value() {
        auto val = std::move(*(return_slot_type*)passing_slot);
        delete (return_slot_type*)passing_slot;
        if((val.index() == 0 && std::get<0>(val).first) || val.index() == 1) {
          get_coro_handle().destroy(); //clean up before exiting.
        }
        if(val.index() == 1) std::rethrow_exception(std::get<1>(val));
        auto& [_, ret] = std::get<0>(val);
        return std::move(ret);
      }
      void unhandled_exception() {
        set_return_exception(std::current_exception());
      }
      template<class T>
      typename definition_t::paused_type run(T&& value) { //must give up your handle's ownership!
        passing_slot = &value;
        get_coro_handle().resume();
        return get_return_value();
      }
      typename definition_t::paused_type run() { //must give up your handle's ownership!
        get_coro_handle().resume();
        return get_return_value();
      }
      coro_type<definition_t> get_return_object() {
        return definition_t::create_object(resume_handle<>{get_coro_handle()});
      }
      auto initial_suspend() {
        return std::suspend_always();
      }
      auto final_suspend() {
        return std::suspend_always();
      }
    };
  protected:
    class waiting_handle {
      promise_type* promise;
      waiting_handle(promise_type* promise):promise(promise){}
      friend promise_type;
    public:
      template<class T = void, class F> requires requires(F&& f, resume_handle<T> resumer) {
        {std::forward<F>(f)(std::move(resumer))} /* noexcept */ -> std::convertible_to<typename definition_t::paused_type>;
      }
      auto resuming_result(F&& f) && {
        auto handle = resume_handle<T>{coro::unique_coro_handle{promise->get_coro_handle()}};
        promise->set_return_value(false, std::forward<F>(f)(std::move(handle)));
        if constexpr(std::is_void_v<T>) {
          return await_from_pointer<void>{};
        } else {
          return await_from_pointer<T>{&promise->passing_slot};
        }
      }
      template<class T = void>
      auto destroying_result(typename definition_t::paused_type&& v) && { //specifies that coroutine is destroyed!
        promise->set_return_value(true, std::move(v));
        return await_with_no_resume<T>{};
      }
      template<class T>
      auto immediate_result(T&& value) {
        return immediate_awaiter<T>(std::forward<T>(value));
      }
      auto immediate_result(){
        return immediate_awaiter<void>();
      }
    };
    class returning_handle {
      promise_type* promise;
      returning_handle(promise_type* promise):promise(promise){}
      friend promise_type;
    public:
      void result(typename definition_t::paused_type&& v) && {
        promise->set_return_value(true, std::move(v));
      }
    };
    /*struct returning_handle {

    };*/
  };
  template<class definition_t>
  struct coro_type : definition_t::starter_base {
    using base = typename definition_t::starter_base;
  public:
    coro_type(base b):base(std::move(b)){}
    coro_type() = default;
  };
}

template<class definition_t, class... Args>
struct std::coroutine_traits<lifted_state_machine::coro_type<definition_t>, Args...> {
  using promise_type = typename lifted_state_machine::coro_base<definition_t>::promise_type;
};
