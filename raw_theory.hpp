#ifndef raw_theory_h
#define raw_theory_h

#include "Templates/TemplateUtility.h"
#include <memory>
#include <variant>
#include <iostream>
#include <functional>

namespace type_theory{
  namespace raw{
    struct empty{};
    enum class function_head{
      constant,
      id,
      native,
      apply,
      builtin
    };

    class value;
    struct value_ptr{
      std::shared_ptr<std::unique_ptr<value> > best;
    };

    struct pair{
      value_ptr first;
      value_ptr second;
    };
    struct placeholder{
      std::size_t index;
    };
    struct lazy_expression{
      value_ptr f;
      value_ptr x;
    };
    struct builtin_definition{
      std::function<value_ptr(value_ptr)> func;
    };

    using primitive_types = utility::class_list<std::size_t, function_head, empty, builtin_definition>;

    class value{
      using storage_types = primitive_types::append<pair, lazy_expression, placeholder>;
      using storage = storage_types::apply<std::variant>;
    public:
      storage data;
      template<class T> requires storage_types::contains<T>
      value(T v):data(std::move(v)){}
      pair const& as_pair() const;
      placeholder const& as_placeholder() const;
      lazy_expression const& as_lazy_expression() const;
      bool is_evaluated() const;
      template<class T> requires primitive_types::contains<T>
      T const& as_primitive() const{ return std::get<T>(data); }
    };

    value_ptr make_pair(value_ptr first, value_ptr second);
    value_ptr make_apply(value_ptr first, value_ptr second);
    value_ptr make_placeholder(std::size_t index);
    template<class T> requires primitive_types::contains<T>
    value_ptr make_primitive(T v){
      return std::make_shared<value>(std::move(v));
    }
    value_ptr make_empty();
    template<function_head h>
    value_ptr make_function_head(){
      static auto ret = std::make_shared<value>(h);
      return ret;
    }
    value_ptr make_id();
    value_ptr make_constant(value_ptr val);
    value_ptr make_native_pair(value_ptr first, value_ptr second);
    value_ptr make_apply_over(value_ptr first, value_ptr second);
    value_ptr make_builtin(std::function<value_ptr(value_ptr)> f);

    value_ptr apply(value_ptr f, value_ptr x);

    std::ostream& operator<<(std::ostream&, empty);
    std::ostream& operator<<(std::ostream&, function_head);
    std::ostream& operator<<(std::ostream&, placeholder);
    std::ostream& operator<<(std::ostream&, builtin_definition const&);
    std::ostream& operator<<(std::ostream&, pair const&);
    std::ostream& operator<<(std::ostream&, lazy_expression const&);
    std::ostream& operator<<(std::ostream&, value const&);
    std::ostream& operator<<(std::ostream&, value_ptr const&);

  }
}

#endif
