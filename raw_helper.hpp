#ifndef raw_helper_h
#define raw_helper_h

#include "raw_theory.hpp"

namespace type_theory{
  namespace raw{
    namespace helper{
      value_ptr convert_placeholder_to_lambda(value_ptr val, std::size_t index);
      template<class... Args> requires (std::is_convertible_v<Args&&, std::size_t> && ...)
      value_ptr convert_placeholder_to_lambda(value_ptr val, std::size_t index, Args&&... args){
        return convert_placeholder_to_lambda(convert_placeholder_to_lambda(val, (std::size_t)args...), index);
      }
      namespace detail{
        template<class first_arg, class... args, class F>
        value_ptr convert_function_to_builtin_impl(F&& func, utility::class_list<first_arg, args...>){
          if constexpr(sizeof...(args) == 0){
            return make_builtin([func = std::forward<F>(func)](value_ptr v) -> value_ptr{
              return make_primitive(func(v->as_primitive<first_arg>()));
            });
          }else{
            return make_builtin([func = std::forward<F>(func)](value_ptr v) -> value_ptr{
              return convert_function_to_builtin_impl(std::bind_front(func, v->as_primitive<first_arg>()), utility::class_list<args...>{});
            });
          }
        }
      }
      template<class F>
      value_ptr convert_function_to_builtin(F&& func){
        using invoke_def = utility::invoke_signature_of<F>;
        using arg_list = typename invoke_def::arg_list;
        return detail::convert_function_to_builtin_impl(func, arg_list{});
      }
    }
  }
}

#endif
