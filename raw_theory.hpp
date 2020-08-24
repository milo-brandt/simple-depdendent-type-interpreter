#define raw_theory_h
#ifndef raw_theory_h
#define raw_theory_h

#include <memory>
#include "Templates/TemplateUtility.h"
#include <unordered_set>
#include <iostream>
#include <functional>
#include <string>
#include "any.hpp"

namespace type_theory::raw{
    struct value;
    using value_ptr = std::shared_ptr<value>;

  enum class type_head{
    primitive_type,
    primitive_empty,
    primitive_size_t,
    function_def
  };
  struct built_in{
    std::size_t args_required;
    std::string name;
    std::function<evaluation_result(std::vector<value_ptr>)> func;
  };

  using primitive_types = utility::class_list<empty, std::size_t, bool, type_head, built_in>;
  template<class T>
  concept primitive = primitive_types::contains<T>;

  struct placeholder{
    std::size_t index;
  };
  struct empty{};
  struct pair{
    value_ptr first;
    value_ptr second;
  };
  struct application{
    value_ptr f;
    value_ptr x;
  };
  struct lambda{
    std::size_t arg_index;
    value_ptr body;
  };

  struct value{
    using any_types = primitive_types::append<pair, application, placeholder, lambda>;
    using storage_variant = any_types::apply<std::variant>;
    storage_variant data;
    template<class T> requires any_types::contains_once<T>
    value(T x):data(std::move(x)){}
    pair const& as_pair() const{ return std::get<pair>(data); }
    bool is_pair() const{ return std::holds_alternative<pair>(data); }
    application const& as_application() const{ return std::get<application>(data); }
    bool is_application() const{ return std::holds_alternative<application>(data); }
    placeholder const& as_placeholder() const{ return std::get<placeholder>(data); }
    bool is_placeholder() const{ return std::holds_alternative<placeholder>(data); }
    lambda const& as_lambda() const{ return std::get<lambda>(data); }
    bool is_lambda() const{ return std::holds_alternative<lambda>(data); }
    template<primitive T>
    T const& as_primitive() const{ return std::get<T>(data); }
    template<primitive T>
    bool is_primitive() const{ return std::holds_alternative<T>(data); }
    bool is_trivially_evaluated() const{ return !is_placeholder() && !is_application(); }
  };


  struct untyped_scope{
    std::unordered_set<std::size_t> used_names;
  };


  value_ptr apply_lambda(value_ptr f, value_ptr x, untyped_scope s = {});
  /*
  evaluation_result step_evaluation(value_ptr input, untyped_scope s = {});
  value_ptr top_eval(value_ptr input, untyped_scope s = {});
  value_ptr deep_eval(value_ptr input, untyped_scope s = {});
*/
  std::ostream& operator<<(std::ostream& o, built_in const&);
  std::ostream& operator<<(std::ostream& o, empty);
  std::ostream& operator<<(std::ostream& o, type_head h);
  std::ostream& operator<<(std::ostream&, value_ptr v);


  template<class T>
  value_ptr make_value(T&& x){ return std::make_shared<value>(std::forward<T>(x)); }
  inline value_ptr make_pair(value_ptr first, value_ptr second){ return make_value(pair{std::move(first), std::move(second)}); }
  template<primitive T>
  value_ptr make_primitive(T value){ return make_value(std::move(value)); }
  inline value_ptr make_placeholder(std::size_t index){ return make_value(placeholder{index}); }
  inline value_ptr make_lazy(value_ptr f, value_ptr x){ return make_value(application{std::move(f), std::move(x)}); }
  inline value_ptr make_lambda(std::size_t index, value_ptr body){ return make_value(lambda{index, std::move(body)}); }
  inline value_ptr make_builtin(std::size_t args, std::string name, std::function<evaluation_result(std::vector<value_ptr>)> func){ return make_lambda(0, make_primitive(built_in(args, std::move(name), std::move(func)))); }
  //inline value_ptr make_identity_type(value_ptr type, value_ptr v1, value_ptr v2){ return make_pair(make_primitive(type_head::identity), make_pair(type, make_pair(v1, v2))); }
  //inline value_ptr make_function_type(value_ptr domain, value_ptr fn){ return make_pair(make_primitive(type_head::function), make_pair(domain, fn)); }
  template<class T>
  value_ptr make_builtin(std::string name, T func){
    using signature = typename utility::invoke_signature_of<T>::arg_list;
    using index_seq = typename signature::index_sequence;
    return [&]<class... Args, std::size_t... Is>(utility::class_list<Args...>, std::index_sequence<Is...>){
      auto lambda = [func = std::move(func)](std::vector<value_ptr> args) -> evaluation_result{
        assert(args.size() == sizeof...(Args));
        if(((!args[Is]->is_trivially_evaluated()) || ...)){
          utility::erase_if(args, [&](value_ptr p){ return p->is_trivially_evaluated(); });
          return evaluation_result::need_values(std::move(args));
        }else{
          auto ret = func(args[Is]->as_primitive<Args>()...);
          if constexpr(std::is_convertible_v<decltype(ret), value_ptr>){
            return evaluation_result::simplified(ret);
          }else{
            return evaluation_result::simplified(make_primitive(ret));
          }
        }
      };
      return make_builtin(sizeof...(Args), std::move(name), lambda);
    }(signature{}, index_seq{});
  }


};

#endif
