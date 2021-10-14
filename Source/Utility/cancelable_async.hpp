#ifndef MDB_CANCELABLE_ASYNC_HPP
#define MDB_CANCELABLE_ASYNC_HPP

#include "async.hpp"

namespace mdb {
  template<class T>
  struct CancelablePromise {
    Promise<T> underlying;
    Future<bool> cancel_signal;
  public:
    template<class F>
    void set_cancelation_listener(F func) {
      cancel_signal.set_listener([func = std::move(func)](bool canceled) mutable {
        if(canceled) {
          std::move(func)();
        }
      });
    }
  };
}

#endif
