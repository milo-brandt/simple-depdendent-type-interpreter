#include <memory>

namespace utility {
  template<class T>
  class fn_once;
  template<class R, class... Args>
  class fn_once<R(Args...)> {
    struct impl_base {
      virtual R call(Args&&...) && = 0;
      virtual ~impl_base() = default;
    };
    template<class F>
    struct impl_specific : impl_base {
      F f;
      impl_specific(F f):f(std::move(f)){}
      R call(Args&&... args) && override {
        return std::move(f)(std::forward<Args>(args)...);
      }
    };
    std::unique_ptr<impl_base> data;
  public:
    fn_once() = default;
    template<class F> requires requires(F f, Args&&... args){ f(std::forward<Args>(args)...); }
    fn_once(F f):data(std::make_unique<impl_specific<F> >(std::move(f))){}
    R operator()(Args... args) && {
      return std::move(*data).call(std::forward<Args>(args)...);
    }
  };
  template<class T>
  class fn_simple; //can be called (mutably) multiple times; cannot be copied.
  template<class R, class... Args>
  class fn_simple<R(Args...)> {
    struct impl_base {
      virtual R call(Args&&...) const& = 0;
      virtual ~impl_base() = default;
    };
    template<class F>
    struct impl_specific : impl_base {
      F f;
      impl_specific(F f):f(std::move(f)){}
      R call(Args&&... args) const& override {
        return f(std::forward<Args>(args)...);
      }
    };
    std::unique_ptr<impl_base> data;
  public:
    fn_simple() = default;
    template<class F> requires requires(F f, Args&&... args){ f(std::forward<Args>(args)...); }
    fn_simple(F f):data(std::make_unique<impl_specific<F> >(std::move(f))){}
    R operator()(Args... args) const& {
      return data->call(std::forward<Args>(args)...);
    }
  };
}
