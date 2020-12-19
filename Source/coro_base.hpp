//
//  coro_base.h
//  Compiley
//
//  Created by Milo Brandt on 5/11/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef coro_base_h
#define coro_base_h

#include <coroutine>
#include <utility>
#include <exception>
#include <variant>
/*namespace std{
  using namespace experimental;
}*/

namespace coro{
    template<class promise_type = void>
    class unique_coro_handle{
    public:
        unique_coro_handle():handle(nullptr){}
        unique_coro_handle(std::nullptr_t):handle(nullptr){}
        unique_coro_handle(std::coroutine_handle<promise_type> handle):handle(handle){}
        unique_coro_handle(unique_coro_handle const&) = delete;
        unique_coro_handle(unique_coro_handle&& other):handle(other.handle){ other.handle = nullptr; }
        unique_coro_handle& operator=(unique_coro_handle const&) = delete;
        unique_coro_handle& operator=(unique_coro_handle&& other){
            if(handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        ~unique_coro_handle(){
            if(handle)
                handle.destroy();
        }
        void resume(){
            auto local = handle;
            handle = nullptr;
            local.resume();
        }
        void resume_keep_ownership(){
          handle.resume();
        }
        void destroy(){
            auto local = handle;
            handle = nullptr;
            local.destroy();
        }
        std::coroutine_handle<promise_type> peek(){
            return handle;
        }
        std::coroutine_handle<promise_type> move(){
            auto ret = handle;
            handle = nullptr;
            return ret;
        }
        promise_type& promise() const{
            return handle.promise();
        }
    private:
        std::coroutine_handle<promise_type> handle;
    };
    template<>
    class unique_coro_handle<void>{
    public:
        unique_coro_handle():handle(nullptr){}
        template<class promise_type>
        unique_coro_handle(std::coroutine_handle<promise_type>&& handle):handle(handle){}
        unique_coro_handle(unique_coro_handle const&) = delete;
        unique_coro_handle(unique_coro_handle&& other):handle(other.handle){ other.handle = nullptr; }
        unique_coro_handle& operator=(unique_coro_handle const&) = delete;
        unique_coro_handle& operator=(unique_coro_handle&& other){
            if(handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        ~unique_coro_handle(){
            if(handle)
                handle.destroy();
        }
        void resume(){
            auto local = handle;
            handle = nullptr;
            local.resume();
        }
        void destroy(){
            auto local = handle;
            handle = nullptr;
            local.destroy();
        }
        std::coroutine_handle<> peek(){
            return handle;
        }
        std::coroutine_handle<> move(){
            auto ret = handle;
            handle = nullptr;
            return ret;
        }
    private:
        std::coroutine_handle<> handle;
    };

    template<class promise_type>
    unique_coro_handle(std::coroutine_handle<promise_type>) -> unique_coro_handle<promise_type>;

    class deferred_routine{
    public:
        struct promise_type;
        using handle = std::coroutine_handle<promise_type>;
        deferred_routine(handle h):h(h){}
        struct promise_type{
            auto get_return_object(){
                return handle::from_promise(*this);
            }
            auto initial_suspend(){
                return std::suspend_always();
            }
            auto final_suspend(){
                return std::suspend_never();
            }
            void return_void(){}
            void unhandled_exception(){
              std::terminate();
            }
        };
        void resume(){
            h.resume();
        }
        void request_resume(){
            resume();
        }
        handle move_handle(){
            return h.move();
        }
        void destroy(){
            h.destroy();
        }
    private:
        unique_coro_handle<promise_type> h;
    };
    inline unique_coro_handle<> do_nothing_routine(){
        return []() -> deferred_routine{ co_return; }().move_handle();
    }
    struct routine{
        struct promise_type{
            routine get_return_object(){
                return {};
            }
            auto initial_suspend(){
                return std::suspend_never();
            }
            auto final_suspend(){
                return std::suspend_never();
            }
            void return_void(){}
            void unhandled_exception(){
              std::terminate();
            }
        };
    };
    template<class T>
    auto apply_co_await(T&& awaiter) requires requires{ std::forward<T>(awaiter).operator co_await(); }{
        return apply_co_await(std::forward<T>(awaiter).operator co_await());
    }
    template<class T>
    auto apply_co_await(T&& awaiter) requires requires{ operator co_await(std::forward<T>(awaiter)); }{
        return operator co_await(std::forward<T>(awaiter));
    }
    template<class T>
    auto apply_co_await(T&& awaiter){
        return std::forward<T>(awaiter);
    }
    template<class F, class A>
    struct mapped_awaiter {
      F map;
      A awaiter;
      bool await_ready() {
        return awaiter.await_ready();
      }
      template<class promise_type>
      auto await_suspend(std::coroutine_handle<promise_type> handle) {
        return awaiter.await_suspend(handle);
      }
      auto await_resume() {
        return map(awaiter.await_resume());
      }
    };
    template<class R, class... Ts>
    struct variant_awaiter {
      std::variant<Ts...> data;
      template<class... Args>
      variant_awaiter(Args&&... args):data{std::forward<Args>(args)...}{}
      bool await_ready(){
        return std::visit([](auto& x) -> bool{ return x.await_ready(); }, data);
      }
      template<class promise_type>
      std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) {
        return std::visit([&](auto& x) -> std::coroutine_handle<>{
          if constexpr(std::is_void_v<decltype(x.await_suspend(handle))>) {
            x.await_suspend(handle);
            return std::noop_coroutine();
          } else if constexpr(std::is_same_v<decltype(x.await_suspend(handle)), bool>) {
            if(x.await_suspend(handle)) {
              return std::noop_coroutine();
            } else {
              return handle;
            }
          } else {
            return x.await_suspend(handle);
          }
        }, data);
      }
      auto await_resume() {
        return std::visit([](auto& x) -> R { return x.await_resume(); }, data);
      }
    };
}

#endif /* coro_base_h */
