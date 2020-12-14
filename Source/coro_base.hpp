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
}

#endif /* coro_base_h */
