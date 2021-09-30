#ifndef MDB_ASYNC_HPP
#define MDB_ASYNC_HPP

#include <optional>
#include <variant>
#include <memory>
#include <type_traits>
#include <vector>
#include "function.hpp"

/*
  Utilities for synchronous promises/futures - mainly, futures allow a callback
  to be set (and replaced) to listen for a value.

  Promises *must* eventually give a value.
*/

namespace mdb {
  namespace detail {
    template<class T>
    struct PromiseSharedState {
      struct PromiseEmpty{};
      struct PromiseDone{};
      using State = std::variant<PromiseEmpty, T, mdb::function<void(T)>, PromiseDone>;
      State state;
      void set_value(T&& value) {
        if(state.index() == 0) {
          state = State{std::in_place_index<1>, std::move(value)};
        } else if(state.index() == 2) {
          std::move(std::get<2>(state))(std::move(value));
          state = State{std::in_place_index<3>};
        } else {
          std::terminate(); //promise set twice!
        }
      }
      void set_listener(mdb::function<void(T)>&& func) {
        if(state.index() == 0 || state.index() == 2) { //allow changing listener
          state = State{std::in_place_index<2>, std::move(func)};
        } else if(state.index() == 1) {
          std::move(func)(std::move(std::get<1>(state)));
          state = State{std::in_place_index<3>};
        } else {
          //nothing to do if promise already finished
        }
      }
      void remove_listener() {
        if(state.index() == 2) {
          state = State{std::in_place_index<0>};
        } else if(state.index() == 3) {
          //nothing to do
        } else{
          std::terminate(); //cannot remove non-set listener
        }
      }
      void remove_promise() {
        if(state.index() == 0 || state.index() == 2) {
          std::terminate(); //broken promise
        }
      }
      bool is_ready() {
        return state.index() == 1;
      }
      T take() {
        T ret = std::move(std::get<1>(state));
        state = State{std::in_place_index<3>};
        return ret;
      }
    };
  };
  template<class T>
  struct PromiseFuturePair;
  template<class T>
  PromiseFuturePair<T> create_promise_future_pair();
  template<class T>
  class Promise { //synchronous promise type
    std::shared_ptr<detail::PromiseSharedState<T> > shared_state;
    Promise(std::shared_ptr<detail::PromiseSharedState<T> > const& shared_state):shared_state(std::move(shared_state)) {}
    friend PromiseFuturePair<T> create_promise_future_pair<T>();
  public:
    Promise(Promise&&) = default;
    Promise(Promise const&) = delete;
    Promise& operator=(Promise&&) = default;
    Promise& operator=(Promise const&) = delete;
    ~Promise() { if(shared_state) shared_state->remove_promise(); }
    void set_value(T value) { shared_state->set_value(std::move(value)); }
  };
  template<class T>
  class Future {
    std::shared_ptr<detail::PromiseSharedState<T> > shared_state;
    Future(std::shared_ptr<detail::PromiseSharedState<T> >&& shared_state):shared_state(std::move(shared_state)) {}
    friend PromiseFuturePair<T> create_promise_future_pair<T>();
  public:
    Future(Future&&) = default;
    Future(Future const&) = delete;
    Future& operator=(Future&&) = default;
    Future& operator=(Future const&) = delete;
    ~Future() = default;
    void listen(mdb::function<void(T)> func) && { //irrevocable listener
      shared_state->set_listener(std::move(func));
      shared_state = nullptr;
    }
    void set_listener(mdb::function<void(T)> func) { shared_state->set_listener(std::move(func)); }
    bool is_ready() const { return shared_state->is_ready(); }
    T take() && { return shared_state->take(); }
    template<class F, class R = std::invoke_result_t<F, T> >
    Future<R> then(F map);
  };
  template<class T>
  struct PromiseFuturePair {
    Promise<T> promise;
    Future<T> future;
  };
  template<class T>
  PromiseFuturePair<T> create_promise_future_pair() {
    auto shared_state = std::make_shared<detail::PromiseSharedState<T> >();
    Promise<T> promise{shared_state};
    Future<T> future{std::move(shared_state)};
    return {
      .promise = std::move(promise),
      .future = std::move(future)
    };
  }
  template<class T>
  template<class F, class R>
  Future<R> Future<T>::then(F map) {
    auto [new_promise, new_future] = create_promise_future_pair<R>();
    std::move(*this).listen([new_promise = std::move(new_promise), map = std::move(map)](T value) mutable {
      new_promise.set_value(std::move(map)(std::move(value)));
    });
    return std::move(new_future);
  }
  template<class... T>
  Future<std::tuple<T...> > collect(Future<T>... futures) {
    auto [promise, future] = create_promise_future_pair<std::tuple<T...> >();
    if constexpr(sizeof...(T) == 0) {
      promise.set_value(std::make_tuple());
    } else {
      struct SharedState {
        std::uint64_t count_left;
        Promise<std::tuple<T...> > promise;
        std::tuple<std::optional<T>...> values;
        SharedState(Promise<std::tuple<T...> > promise):count_left(sizeof...(T)), promise(std::move(promise)) {}
        void finish() {
          std::apply([&](std::optional<T>&... values) {
            promise.set_value(std::make_tuple(std::move(*values)...));
          }, values);
        }
        void decrement() {
          if(--count_left == 0) {
            finish();
          }
        }
      };
      auto shared = std::make_shared<SharedState>(std::move(promise));
      std::apply([&](std::optional<T>&... values) {
        (std::move(futures).listen([shared, &value = values](T v) mutable {
          value = std::move(v);
          shared->decrement();
        }) , ...);
      }, shared->values);
    }
    return std::move(future);
  }
  template<class T>
  Future<std::vector<T> > collect(std::vector<Future<T> > futures) {
    auto [promise, future] = create_promise_future_pair<std::vector<T> >();
    if(futures.empty()) {
      promise.set_value(std::vector<T>{});
    } else {
      struct SharedState {
        std::uint64_t count_left;
        Promise<std::vector<T> > promise;
        std::vector<std::optional<T> > values;
        SharedState(std::uint64_t count, Promise<std::vector<T> > promise):count_left(count), promise(std::move(promise)), values(count, std::optional<T>{}) {}
        void finish() {
          std::vector<T> vec;
          vec.reserve(values.size());
          for(auto& value : values) {
            vec.push_back(std::move(*value));
          }
          promise.set_value(std::move(vec));
        }
        void decrement() {
          if(--count_left == 0) {
            finish();
          }
        }
      };
      auto shared = std::make_shared<SharedState>(futures.size(), std::move(promise));
      for(std::size_t i = 0; i < futures.size(); ++i) {
        std::move(futures[i]).listen([shared, i](T v) {
          shared->values[i] = std::move(v);
          shared->decrement();
        });
      }
    }
    return std::move(future);
  }
}

#endif
