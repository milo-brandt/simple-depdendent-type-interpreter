#include "template_utility.hpp"
#include "coro_base.hpp"
#include <variant>

/*
Represents coroutines that can co_await, co_yield, or co_return some (finite) set of possibilities and then be resumed given appropriate further input.


*/


namespace lifted_state_machine {
  template<class promise_type, class resume_type = void>
  class resume_handle {
    coro::unique_coro_handle<promise_type> raw_handle;
    explicit resume_handle(coro::unique_coro_handle<promise_type> raw_handle):raw_handle(std::move(raw_handle)){}
    void set_handle(coro::unique_coro_handle<promise_type> raw_handle){ this->raw_handle = std::move(raw_handle); }
    friend promise_type;
  public:
    resume_handle() = default;
    auto operator()(resume_type arg) {
      auto& promise = raw_handle.promise();
      return promise.run(std::move(raw_handle), std::move(arg));
    }
  };
  template<class promise_type>
  class resume_handle<promise_type, void> {
    coro::unique_coro_handle<promise_type> raw_handle;
    explicit resume_handle(coro::unique_coro_handle<promise_type> raw_handle):raw_handle(std::move(raw_handle)){}
    void set_handle(coro::unique_coro_handle<promise_type> raw_handle){ this->raw_handle = std::move(raw_handle); }
    friend promise_type;
  public:
    resume_handle() = default;
    auto operator()() {
      auto& promise = raw_handle.promise();
      return promise.run(std::move(raw_handle));
    }
  };
  template<class getter>
  struct argument_passer {
    getter get;
    bool await_ready(){ return false; }
    template<class T>
    void await_suspend(T&&){}
    auto await_resume() {
      return get();
    }
  };
  template<class getter>
  argument_passer(getter) -> argument_passer<getter>;
  template<class promise_t, class awaitable>
  struct await_implementer {
    using resume_type = typename awaitable::resume_type;
    using return_type = std::pair<awaitable, resume_handle<promise_t, resume_type> >;
    promise_t* promise(){ return (promise_t*)this; }
    auto await_transform(awaitable a) {
      promise()->template set_result(std::make_pair(std::move(a), resume_handle<promise_t, resume_type>{}));
      return argument_passer{[p = promise()](){
        if constexpr(std::is_void_v<resume_type>){
          return;
        } else {
          return std::move(*std::get<resume_type*>(p->passing_slot));
        }
      }};
    }
  };
  template<class specification>
  class promise;
  template<class... awaitables>
  class promise<utility::class_list<awaitables...> > : await_implementer<promise<utility::class_list<awaitables...> >, awaitables>... {
  public:
    using unique_handle = coro::unique_coro_handle<promise>;
    using return_type = std::variant<std::monostate, typename await_implementer<promise, awaitables>::return_type...>;
    std::variant<std::monostate, std::exception_ptr, return_type, typename awaitables::resume_type*...> passing_slot;
    using await_implementer<promise<utility::class_list<awaitables...> >, awaitables>::await_transform...;
    void set_result(return_type&& a) {
      passing_slot = std::move(a);
    }
    return_type get_return_value(unique_handle handle) {
      if(std::holds_alternative<std::exception_ptr>(passing_slot)) std::rethrow_exception(std::get<std::exception_ptr>(passing_slot));
      return std::visit<return_type>([&]<class T>(T&& value){
        if constexpr(std::is_same_v<T, std::monostate>){
          return value;
        } else {
          value.second.set_handle(std::move(handle));
          return std::move(value);
        }
      }, std::move(std::get<return_type>(passing_slot)));
    }
    template<class resume_type>
    return_type run(unique_handle handle, resume_type&& resume) {
      passing_slot = &resume;
      std::coroutine_handle<promise>::from_promise(*this).resume();
      return get_return_value(std::move(handle));
    }
    return_type run(unique_handle handle) {
      std::coroutine_handle<promise>::from_promise(*this).resume();
      return get_return_value(std::move(handle));
    }
    resume_handle<promise> get_return_object(){
        return resume_handle<promise>{coro::unique_coro_handle<promise>{std::coroutine_handle<promise>::from_promise(*this)}};
    }
    auto initial_suspend(){
        return std::suspend_always();
    }
    auto final_suspend(){
        return std::suspend_always();
    }
    void return_void(){
      passing_slot = return_type{};
    }
    void unhandled_exception(){
      passing_slot = std::current_exception();
    }
  };
}
template<class promise_type_t, class... Args>
struct std::coroutine_traits<lifted_state_machine::resume_handle<promise_type_t>, Args...> {
  using promise_type = promise_type_t;
};
