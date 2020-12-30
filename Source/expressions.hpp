#ifndef EXPRESSIONS_HPP
#define EXPRESSIONS_HPP

#include <variant>
#include <memory>
#include <algorithm>
#include <functional> //for std::equal_to
#include "template_utility.hpp"

namespace expressionz {
  template<class F, class X>
  struct apply;
  template<class B>
  struct abstract;
  struct argument;
  template<class T>
  class expression;
  template<class T>
  using local_expression = std::variant<T, apply<expression<T>, expression<T> >, abstract<expression<T> >, argument>;
  enum class local_type : std::size_t {
    value = 0,
    apply = 1,
    abstract = 2,
    argument = 3
  };
  template<local_type t, class primitive_type>
  bool holds_alternative(local_expression<primitive_type> const& expr) {
    return expr.index() == (std::size_t)t;
  }
  template<local_type t, class primitive_type>
  decltype(auto) get(local_expression<primitive_type>& expr) {
    return std::get<(std::size_t)t>(expr);
  }
  template<local_type t, class primitive_type>
  decltype(auto) get(local_expression<primitive_type> const& expr) {
    return std::get<(std::size_t)t>(expr);
  }
  template<local_type t, class primitive_type>
  decltype(auto) get_if(local_expression<primitive_type>* expr) {
    return std::get_if<(std::size_t)t>(expr);
  }
  template<local_type t, class primitive_type>
  decltype(auto) get_if(local_expression<primitive_type> const* expr) {
    return std::get_if<(std::size_t)t>(expr);
  }

  template<class T>
  struct apply_to_arg {
    std::size_t head;
    std::vector<expression<T> > args;
  };
  template<class T>
  struct apply_to_primitive {
    T head;
    std::vector<expression<T> > args;
  };
  template<class T>
  using normal_expression = std::variant<abstract<expression<T> >, apply_to_arg<T>, apply_to_primitive<T> >;
  enum class normal_type : std::size_t {
    abstract = 0,
    apply_to_arg = 1,
    apply_to_primitive = 2
  };
  template<normal_type t, class primitive_type>
  bool holds_alternative(normal_expression<primitive_type> const& expr) {
    return expr.index() == (std::size_t)t;
  }
  template<normal_type t, class primitive_type>
  decltype(auto) get(normal_expression<primitive_type>& expr) {
    return std::get<(std::size_t)t>(expr);
  }
  template<normal_type t, class primitive_type>
  decltype(auto) get(normal_expression<primitive_type> const& expr) {
    return std::get<(std::size_t)t>(expr);
  }
  template<normal_type t, class primitive_type>
  decltype(auto) get_if(normal_expression<primitive_type>* expr) {
    return std::get_if<(std::size_t)t>(expr);
  }
  template<normal_type t, class primitive_type>
  decltype(auto) get_if(normal_expression<primitive_type> const* expr) {
    return std::get_if<(std::size_t)t>(expr);
  }

  template<class T>
  class expression {
  public:
    expression(T value);
    expression(argument arg);
    template<class B>
    expression(abstract<B> abstraction);
    template<class F, class X>
    expression(apply<F, X>);
    template<class V>
    expression(V);
    expression(expression const&) = default;
    expression(expression&&) = default;
    expression& operator=(expression const&) = default;
    expression& operator=(expression&&) = default;
    local_expression<T> local_form() const;
    normal_expression<T> normal_form() const;
  private:
    struct impl_base;
    struct detail;
    std::shared_ptr<impl_base> data;
  };

  template<class F, class X>
  struct apply{
    F f;
    X x;
    apply(F f, X x):f(std::move(f)),x(std::move(x)){}
  };
  template<class F, class X>
  apply(F, X) -> apply<F, X>;
  template<class B>
  struct abstract {
    B body;
    abstract(B body):body(std::move(body)){}
  };
  template<class B>
  abstract(B) -> abstract<B>;
  struct argument {
    std::size_t index;
    argument(std::size_t index):index(index){}
  };

  template<class T>
  struct expression<T>::impl_base {
    virtual local_expression<T> simplify() const = 0;
    virtual ~impl_base() = default;
  };
  template<class T>
  struct expression<T>::detail {
    struct value_impl : impl_base {
      T value;
      value_impl(T value):value(std::move(value)){}
      local_expression<T> simplify() const override {
        return local_expression<T>{std::in_place_index<0>, value};
      }
    };
    struct argument_impl : impl_base {
      std::size_t index;
      argument_impl(std::size_t index):index(index){}
      local_expression<T> simplify() const override {
        return local_expression<T>{std::in_place_index<3>, argument(index)};
      }
    };
    struct apply_impl : impl_base {
      expression<T> f;
      expression<T> x;
      apply_impl(expression<T> f, expression<T> x):f(std::move(f)),x(std::move(x)){}
      local_expression<T> simplify() const override {
        return local_expression<T>{std::in_place_index<1>, apply(f, x)};
      }
    };
    struct abstract_impl : impl_base {
      expression<T> body;
      abstract_impl(expression<T> body):body(std::move(body)){}
      local_expression<T> simplify() const override {
        return local_expression<T>{std::in_place_index<2>, abstract(body)};
      }
    };
    template<class V>
    struct special_impl : impl_base {
      V value;
      special_impl(V value):value(std::move(value)){}
      local_expression<T> simplify() const override {
        return value.template simplify_as<T>();
      }
    };
    template<class V>
    static std::shared_ptr<impl_base> handle_arbitrary_input(V v) {
      constexpr bool has_simplifier = requires{
        v.template simplify_as<T>();
      };
      constexpr bool is_convertible_to_value = requires{
        T(std::move(v));
      };
      static_assert(has_simplifier || is_convertible_to_value, "cannot construct expression");
      if constexpr(has_simplifier) {
        return std::make_shared<special_impl<V> >(std::move(v));
      } else {
        return std::make_shared<value_impl>(T(std::move(v)));
      }
    }
  };
  template<class T>
  expression<T>::expression(T value):data(std::make_shared<typename detail::value_impl>(std::move(value))){}
  template<class T>
  expression<T>::expression(argument arg):data(std::make_shared<typename detail::argument_impl>(arg.index)){}
  template<class T>
  template<class F, class X>
  expression<T>::expression(apply<F, X> application):data(std::make_shared<typename detail::apply_impl>(std::move(application.f), std::move(application.x))){}
  template<class T>
  template<class B>
  expression<T>::expression(abstract<B> abstraction):data(std::make_shared<typename detail::abstract_impl>(std::move(abstraction.body))){}
  template<class T>
  template<class V>
  expression<T>::expression(V value):data(detail::handle_arbitrary_input(std::move(value))){}
  template<class T>
  auto expression<T>::local_form() const -> local_expression<T> {
    return data->simplify();
  }
  namespace detail {
    class all_types_ok {
    public:
      all_types_ok() = delete;
      template<class T>
      operator T() const { std::terminate(); } //unreachable
    };
    template<class A, class B>
    struct common_type_getter {
      using type = std::common_type_t<A, B>;
    };
    template<class A>
    struct common_type_getter<all_types_ok, A> {
      using type = A;
    };
    template<class A>
    struct common_type_getter<A, all_types_ok> {
      using type = A;
    };
    template<>
    struct common_type_getter<all_types_ok, all_types_ok> {
      using type = all_types_ok;
    };
    template<class T>
    struct type_wrapper {
      using type = T;
    };

    template<class T>
    struct preferred_type_getter {
      static auto type_getter() {
        if constexpr(requires{ typename T::preferred_type; }) {
          return type_wrapper<typename T::preferred_type>{};
        } else {
          return type_wrapper<T>{};
        }
      };
      using type = typename decltype(type_getter())::type;
    };
    template<class F, class X>
    struct preferred_type_getter<apply<F, X> > {
      using type = typename common_type_getter<typename preferred_type_getter<F>::type, typename preferred_type_getter<X>::type>::type;
    };
    template<class B>
    struct preferred_type_getter<abstract<B> > {
      using type = typename preferred_type_getter<B>::type;
    };
    template<>
    struct preferred_type_getter<argument> {
      using type = all_types_ok;
    };
    template<class T>
    struct preferred_type_getter<expression<T> > {
      using type = T;
    };
  };
  template<class T>
  using preferred_type_of = typename detail::preferred_type_getter<T>::type;
  template<class T, bool b = requires{ typename preferred_type_of<T>; }>
  struct with_preferred_type_of{};
  template<class T>
  struct with_preferred_type_of<T, true> {
    using preferred_type = preferred_type_of<T>;
  };

  template<class T>
  expression(T) -> expression<preferred_type_of<T> >;

  template<class B>
  struct deepen : with_preferred_type_of<B> {
    std::size_t depth_added;
    std::size_t depth_ignored = 0;
    B target;
    deepen(std::size_t depth_added, B target):depth_added(depth_added),target(std::move(target)){}
    deepen(std::size_t depth_added, std::size_t depth_ignored, B target):depth_added(depth_added),depth_ignored(depth_ignored),target(std::move(target)){}
    template<class T> requires requires{ expression<T>(target); }
    local_expression<T> simplify_as() const {
      return std::visit(utility::overloaded{
        [&](T&& value) -> local_expression<T> {
          return std::move(value);
        },
        [&](apply<expression<T>, expression<T> >&& application) -> local_expression<T> {
          return apply(
            (expression<T>)expressionz::deepen(depth_added, depth_ignored, std::move(application.f)),
            (expression<T>)expressionz::deepen(depth_added, depth_ignored, std::move(application.x))
          );
        },
        [&](abstract<expression<T> >&& abstraction) -> local_expression<T> {
          return abstract((expression<T>)expressionz::deepen(depth_added, depth_ignored + 1, std::move(abstraction.body)));
        },
        [&](argument&& arg) -> local_expression<T> {
          if(arg.index < depth_ignored) {
            return arg;
          } else {
            return argument(arg.index + depth_added);
          }
        }
      }, expression<T>(target).local_form());
    }
  };
  template<class B>
  deepen(std::size_t, B) -> deepen<B>;
  template<class B>
  deepen(std::size_t, std::size_t, B) -> deepen<B>;
  template<class A, class B>
  struct substitute : with_preferred_type_of<apply<A, B> > {
    std::size_t depth = 0;
    A arg;
    B target;
    substitute(A arg, B target):arg(std::move(arg)),target(std::move(target)){}
    substitute(std::size_t depth, A arg, B target):depth(depth),arg(std::move(arg)),target(std::move(target)){}
    template<class T> requires requires{ expression<T>(target); expression<T>(arg); }
    local_expression<T> simplify_as() const {
      expression<T> expr_arg(arg);
      return std::visit(utility::overloaded{
        [&](T&& value) -> local_expression<T> {
          return std::move(value);
        },
        [&](apply<expression<T>, expression<T> >&& application) -> local_expression<T> {
          return apply(
            (expression<T>)expressionz::substitute(depth, expr_arg, std::move(application.f)),
            (expression<T>)expressionz::substitute(depth, expr_arg, std::move(application.x))
          );
        },
        [&](abstract<expression<T> >&& abstraction) -> local_expression<T> {
          return abstract((expression<T>)expressionz::substitute(depth + 1, expr_arg, std::move(abstraction.body)));
        },
        [&](argument&& arg) -> local_expression<T> {
          if(arg.index < depth) {
            return arg;
          } else if(arg.index == depth) {
            return expression<T>(deepen(depth, expr_arg)).local_form();
          } else {
            return argument{arg.index - 1};
          }
        }
      }, expression<T>(target).local_form());
    }
  };
  template<class A, class B>
  substitute(A, B) -> substitute<A, B>;
  template<class A, class B>
  substitute(std::size_t, A, B) -> substitute<A, B>;

  template<class F, class V>
  struct bind : with_preferred_type_of<std::invoke_result_t<F const&, V&&> > {
   std::size_t depth = 0;
   F map;
   expression<V> target;
   bind(F map, expression<V> target):map(std::move(map)),target(std::move(target)){}
   bind(std::size_t depth, F map, expression<V> target):map(std::move(map)),target(std::move(target)){}
   template<class T> requires requires(V value){ expression<T>(map(std::move(value))); }
   local_expression<T> simplify_as() const {
     return std::visit(utility::overloaded{
       [&](V&& value) -> local_expression<T> {
         return expression<T>(deepen(depth, map(std::move(value)))).local_form();
       },
       [&](apply<expression<V>, expression<V> >&& application) -> local_expression<T> {
         return apply(
           (expression<T>)expressionz::bind(depth, map, std::move(application.f)),
           (expression<T>)expressionz::bind(depth, map, std::move(application.x))
         );
       },
       [&](abstract<expression<V> >&& abstraction) -> local_expression<T> {
         return abstract((expression<T>)expressionz::bind(depth + 1, map, std::move(abstraction.body)));
       },
       [&](argument&& arg) -> local_expression<T> {
         return arg;
       }
     }, target.local_form());
   }
  };
  template<class F, class V>
  bind(F, expression<V>) -> bind<F, V>;
  template<class F, class V>
  bind(F, V) -> bind<F, preferred_type_of<V> >;
  template<class F, class V>
  bind(std::size_t, F, expression<V>) -> bind<F, V>;
  template<class F, class V>
  bind(std::size_t, F, V) -> bind<F, preferred_type_of<V> >;

  template<class A, class B> //bug - calls .local_form multiple times on arguments.
  struct substitute_many : with_preferred_type_of<apply<A, B> > {
    std::size_t depth;
    std::shared_ptr<std::vector<A> > args;
    B target;
    substitute_many(std::size_t depth, std::shared_ptr<std::vector<A> > args, B target):depth(depth),args(std::move(args)), target(std::move(target)) {}
    substitute_many(std::size_t depth, std::vector<A> args, B target):depth(depth),args(std::make_shared<std::vector<A> >(std::move(args))), target(std::move(target)) {}
    substitute_many(std::vector<A> args, B target):substitute_many(0, std::move(args), std::move(target)){}
    template<class T> requires requires{ expression<T>(target); expression<T>((*args)[0]); }
    local_expression<T> simplify_as() const {
      return std::visit(utility::overloaded{
        [&](T&& value) -> local_expression<T> {
          return std::move(value);
        },
        [&](apply<expression<T>, expression<T> >&& application) -> local_expression<T> {
          return apply(
            (expression<T>)expressionz::substitute_many(depth, args, std::move(application.f)),
            (expression<T>)expressionz::substitute_many(depth, args, std::move(application.x))
          );
        },
        [&](abstract<expression<T> >&& abstraction) -> local_expression<T> {
          return abstract((expression<T>)expressionz::substitute_many(depth + 1, args, std::move(abstraction.body)));
        },
        [&](argument&& arg) -> local_expression<T> {
          if(arg.index < depth) {
            return arg;
          } else if(arg.index < depth + args->size()) {
            return expression<T>(deepen(depth, (*args)[arg.index - depth])).local_form();
          } else {
            return argument{arg.index - args->size()};
          }
        }
      }, expression<T>(target).local_form());
    }
  };
  template<class T>
  normal_expression<T> expression<T>::normal_form() const {
    expression<T> head = *this;
    std::vector<expression<T> > rev_args;
    while(true) {
      auto local = head.local_form();
      if(auto* apply = get_if<local_type::apply>(&local)) {
        rev_args.push_back(std::move(apply->x));
        head = std::move(apply->f);
      } else if(auto* abstract = get_if<local_type::abstract>(&local)) {
        if(rev_args.size() > 0) {
          auto back = std::move(rev_args.back());
          rev_args.pop_back();
          head = substitute(std::move(back), std::move(abstract->body));
        } else {
          return *abstract;
        }
      } else if(auto* arg = get_if<local_type::argument>(&local)) {
        std::reverse(rev_args.begin(), rev_args.end());
        return apply_to_arg<T>{arg->index, std::move(rev_args)};
      } else {
        std::reverse(rev_args.begin(), rev_args.end());
        auto& v = get<local_type::value>(local);
        return apply_to_primitive<T>{std::move(v), std::move(rev_args)};
      }
    }
  }
  template<class T, class Cmp = std::equal_to<T> >
  bool __deep_literal_compare(expression<T> lhs, expression<T> rhs, Cmp&& cmp = {}) { //don't use - it expands expressions without saving results anywhere
    auto l = lhs.local_form();
    auto r = rhs.local_form();
    if(auto* apply_l = get_if<local_type::apply>(&l)) {
      if(auto* apply_r = get_if<local_type::apply>(&r)) {
        return __deep_literal_compare(apply_l->f, apply_r->f) && __deep_literal_compare(apply_l->x, apply_r->x);
      } else {
        return false;
      }
    } else if(auto* abstract_l = get_if<local_type::abstract>(&l)) {
      if(auto* abstract_r = get_if<local_type::abstract>(&r)) {
        return __deep_literal_compare(abstract_l->body, abstract_r->body);
      } else {
        return false;
      }
    } else if(auto* arg_l = get_if<local_type::argument>(&l)) {
      if(auto* arg_r = get_if<local_type::argument>(&r)) {
        return arg_l->index == arg_r->index;
      } else {
        return false;
      }
    } else if(auto* arg_l = get_if<local_type::argument>(&l)) {
      if(auto* arg_r = get_if<local_type::argument>(&r)) {
        return arg_l->index == arg_r->index;
      } else {
        return false;
      }
    } else if(auto* val_l = get_if<local_type::value>(&l)) {
      if(auto* val_r = get_if<local_type::value>(&r)) {
        return cmp(*val_l, *val_r);
      } else {
        return false;
      }
    }
    std::terminate();
  }
}

#endif
