#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <memory>
#include <variant>
#include <concepts>
#include <iostream>
#include <vector>
#include <type_traits>
#include "template_utility.hpp"

namespace expressions {
  template<class primitive_type>
  struct apply_t;
  template<class primitive_type>
  struct abstract_t;
  template<class primitive_type>
  struct argument_t;
  template<class primitive_type>
  struct value_t;

  template<class primitive_type>
  using local_expression = std::variant<apply_t<primitive_type>, abstract_t<primitive_type>, argument_t<primitive_type>, value_t<primitive_type> >;

  template<class primitive_t>
  struct expression_base {
  public:
    using primitive_type = primitive_t;
    virtual local_expression<primitive_t> reduce_to_local_expression() const = 0;
    virtual ~expression_base() = default;
  };
  template<class primitive_type>
  using expression = std::shared_ptr<expression_base<primitive_type> >;
  template<class T, class... Args> requires std::is_constructible_v<T, Args&&...> && requires{
    typename T::primitive_type;
    requires std::is_base_of_v<expression_base<typename T::primitive_type>, T>;
  }
  expression<typename T::primitive_type> make_expression(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  template<class primitive_type>
  struct apply_t : expression_base<primitive_type> {
    expression<primitive_type> f;
    expression<primitive_type> x;
    apply_t(expression<primitive_type> f, expression<primitive_type> x):f(std::move(f)),x(std::move(x)){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      return *this;
    }
  };
  template<class primitive_type>
  struct abstract_t : expression_base<primitive_type> {
    expression<primitive_type> body;
    abstract_t(expression<primitive_type> body):body(std::move(body)){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      return *this;
    }
  };
  template<class primitive_type>
  struct argument_t : expression_base<primitive_type> {
    std::size_t index;
    argument_t(std::size_t index):index(index){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      return *this;
    }
  };
  template<class primitive_type>
  struct value_t : expression_base<primitive_type> {
    primitive_type value;
    value_t(primitive_type value):value(std::move(value)){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      return *this;
    }
  };
  namespace detail {
    template<class expr_t>
    struct __is_expression_helper {
      constexpr static bool value = false;
    };
    template<class T>
    struct __is_expression_helper<expression<T> > {
      constexpr static bool value = true;
      using primitive_type = T;
    };
    template<class expr_t>
    concept is_expression = __is_expression_helper<expr_t>::value;
    template<class expr_t>
    using expression_type = typename __is_expression_helper<expr_t>::primitive_type;
  }
  template<class input_primitive_type, std::invocable<input_primitive_type> map_t> requires detail::is_expression<std::invoke_result_t<map_t const&, input_primitive_type> >
  struct bind : expression_base<detail::expression_type<std::invoke_result_t<map_t const&, input_primitive_type> > > {
    using primitive_type = detail::expression_type<std::invoke_result_t<map_t const&, input_primitive_type> >;
    map_t map;
    expression<input_primitive_type> target;
    bind(map_t map, expression<input_primitive_type> target):map(std::move(map)),target(std::move(target)){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      auto local = target->reduce_to_local_expression();
      std::visit(utility::overloaded{
        [&](apply_t<input_primitive_type> apply) -> local_expression<primitive_type> {
          return apply_t<primitive_type>{bind{map, apply.f}, bind{map, apply.x}};
        },
        [&](abstract_t<input_primitive_type> abstract) -> local_expression<primitive_type> {
          return abstract_t<primitive_type>{bind{map, abstract.body}};
        },
        [&](argument_t<input_primitive_type> argument) -> local_expression<primitive_type> {
          return argument_t<primitive_type>{argument.index};
        },
        [&](value_t<input_primitive_type> value) -> local_expression<primitive_type> {
          return map(value)->reduce_to_local_expression();
        }
      }, std::move(local));
    }
  };
  template<class primitive_type>
  struct deepen_t: expression_base<primitive_type> {
    std::size_t depth; //how much to change arg indices by
    std::size_t bound_arguments; //how many arguments should not be changed
    expression<primitive_type> target;
    deepen_t(std::size_t depth, std::size_t bound_arguments, expression<primitive_type> target):depth(depth),bound_arguments(bound_arguments),target(std::move(target)){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      auto local = target->reduce_to_local_expression();
      return std::visit(utility::overloaded{
        [&](apply_t<primitive_type> apply) -> local_expression<primitive_type> {
          return apply_t<primitive_type>{make_expression<deepen_t>(depth, bound_arguments, apply.f), make_expression<deepen_t>(depth, bound_arguments, apply.x)};
        },
        [&](abstract_t<primitive_type> abstract) -> local_expression<primitive_type> {
          return abstract_t<primitive_type>{make_expression<deepen_t>(depth, bound_arguments + 1, abstract.body)};
        },
        [&](argument_t<primitive_type> argument) -> local_expression<primitive_type> {
          if(argument.index < bound_arguments) {
            return argument_t<primitive_type>{argument.index};
          } else {
            return argument_t<primitive_type>{argument.index + depth};
          }
        },
        [&](value_t<primitive_type> value) -> local_expression<primitive_type> {
          return value;
        }
      }, std::move(local));
    }
  };
  template<class primitive_type>
  struct substitute_t : expression_base<primitive_type> {
    std::size_t index;
    expression<primitive_type> argument;
    expression<primitive_type> target;
    substitute_t(std::size_t index, expression<primitive_type> argument, expression<primitive_type> target):index(index),argument(std::move(argument)),target(std::move(target)){}
    local_expression<primitive_type> reduce_to_local_expression() const override {
      auto local = target->reduce_to_local_expression();
      return std::visit(utility::overloaded{
        [&](apply_t<primitive_type> apply) -> local_expression<primitive_type> {
          return apply_t<primitive_type>{make_expression<substitute_t>(index, argument, apply.f), make_expression<substitute_t>(index, argument, apply.x)};
        },
        [&](abstract_t<primitive_type> abstract) -> local_expression<primitive_type> {
          return abstract_t<primitive_type>{make_expression<substitute_t>(index + 1, argument, abstract.body)};
        },
        [&](argument_t<primitive_type> arg) -> local_expression<primitive_type> {
          if(arg.index < index) {
            return argument_t<primitive_type>{arg.index};
          } else if(arg.index == index) {
            return deepen_t<primitive_type>{index, 0, argument}.reduce_to_local_expression();
          } else {
            return argument_t<primitive_type>{arg.index - 1};
          }
        },
        [&](value_t<primitive_type> value) -> local_expression<primitive_type> {
          return value;
        }
      }, std::move(local));
    }
  };
  template<class primitive_type>
  struct debug_print{
    expression<primitive_type> data;
  };
  template<class primitive_type> debug_print(expression<primitive_type>) -> debug_print<primitive_type>;
  template<class primitive_type> requires requires(std::ostream& o, primitive_type const& x){ o << x; }
  std::ostream& operator<<(std::ostream& o, debug_print<primitive_type> const& x) {
    auto local = x.data->reduce_to_local_expression();
    std::visit(utility::overloaded{
      [&](apply_t<primitive_type> apply) {
        o << "(" << debug_print{apply.f} << " " << debug_print{apply.x} << ")";
      },
      [&](abstract_t<primitive_type> abstract) {
        o << "lambda(" << debug_print{abstract.body} << ")";
      },
      [&](argument_t<primitive_type> arg) {
        o << "arg(" << arg.index << ")";
      },
      [&](value_t<primitive_type> value) {
        o << value.value;
      }
    }, std::move(local));
    return o;
  }

  template<class primitive_type>
  expression<primitive_type> apply(expression<primitive_type> f, expression<primitive_type> x) {
    return make_expression<apply_t<primitive_type> >(std::move(f), std::move(x));
  }
  template<class primitive_type>
  expression<primitive_type> abstract(expression<primitive_type> body) {
    return make_expression<abstract_t<primitive_type> >(std::move(body));
  }
  template<class primitive_type>
  expression<primitive_type> argument(std::size_t index) {
    return make_expression<argument_t<primitive_type> >(index);
  }
  template<class primitive_type>
  expression<primitive_type> value(primitive_type v) {
    return make_expression<value_t<primitive_type> >(std::move(v));
  }
  template<class primitive_type>
  bool expression_uses_argument(expression<primitive_type> expr, std::size_t index) {
    auto local = expr->reduce_to_local_expression();
    return std::visit(utility::overloaded{
      [&](apply_t<primitive_type> apply) {
        return expression_uses_argument(apply.f, index) || expression_uses_argument(apply.x, index);
      },
      [&](abstract_t<primitive_type> abstract) {
        return expression_uses_argument(abstract.body, index + 1);
      },
      [&](argument_t<primitive_type> arg) {
        return arg.index == index;
      },
      [&](value_t<primitive_type> value) {
        return false;
      }
    }, std::move(local));

  }
  template<class primitive_type>
  std::optional<expression<primitive_type> > remove_trivial_abstractions(abstract_t<primitive_type> base) {
    auto body = base.body->reduce_to_local_expression();
    return std::visit(utility::overloaded{
      [&](apply_t<primitive_type> apply) -> std::optional<expression<primitive_type> > {
        auto x = apply.x->reduce_to_local_expression();
        if(std::holds_alternative<argument_t<primitive_type> >(x) && std::get<argument_t<primitive_type> >(x).index == 0 && !expression_uses_argument(apply.f, 0)) {
          return (expression<primitive_type>)std::make_shared<deepen_t<primitive_type> >(-std::size_t(1), 0, apply.f);
        } else {
          return std::nullopt;
        }
        std::terminate(); //gcc warns otherwise???
      },
      [&](abstract_t<primitive_type> abstract) -> std::optional<expression<primitive_type> > {
        auto body = remove_trivial_abstractions(abstract);
        if(!body) return std::nullopt;
        return remove_trivial_abstractions(abstract_t<primitive_type>(*body));
      },
      [&](argument_t<primitive_type> arg) -> std::optional<expression<primitive_type> > {
        return std::nullopt;
      },
      [&](value_t<primitive_type> value) -> std::optional<expression<primitive_type> > {
        return std::nullopt;
      }
    }, std::move(body));
  }
  template<class primitive_type>
  struct beta_free_application {
    std::variant<argument_t<primitive_type>, value_t<primitive_type> > head;
    std::vector<expression<primitive_type> > rev_args;
    expression<primitive_type> fold() && {
      expression<primitive_type> ret = std::visit([]<class T>(T value) -> expression<primitive_type> { return std::make_shared<T>(std::move(value)); }, std::move(head));
      while(!rev_args.empty()) {
        auto back = std::move(rev_args.back());
        rev_args.pop_back();
        ret = apply(ret, back);
      }
      return ret;
    }
  };
  #include <iostream>
  template<class primitive_type>
  using beta_free_expression = std::variant<beta_free_application<primitive_type>, abstract_t<primitive_type> >;
  template<class primitive_type>
  beta_free_expression<primitive_type> reduce_to_beta_free_head(expression<primitive_type> head, std::vector<expression<primitive_type> > rev_args = {}) {
    while(true) {
      auto local = head->reduce_to_local_expression();
      auto ret = std::visit(utility::overloaded{
        [&](apply_t<primitive_type> apply) -> std::optional<beta_free_expression<primitive_type> > {
          rev_args.push_back(apply.x);
          head = apply.f;
          return {};
        },
        [&](abstract_t<primitive_type> abstract) -> std::optional<beta_free_expression<primitive_type> > {
          if(rev_args.size() > 0) {
            auto back = std::move(rev_args.back());
            rev_args.pop_back();
            head = std::make_shared<substitute_t<primitive_type> >(0, std::move(back), std::move(abstract.body));
            return {};
          } else {
            auto reduced = remove_trivial_abstractions(abstract);
            if(reduced) {
              head = std::move(*reduced);
              return {};
            } else {
              return abstract;
            }
          }
        },
        [&](argument_t<primitive_type> arg) -> std::optional<beta_free_expression<primitive_type> > {
          return beta_free_application<primitive_type>{std::move(arg), std::move(rev_args)};
        },
        [&](value_t<primitive_type> value) -> std::optional<beta_free_expression<primitive_type> > {
          return beta_free_application<primitive_type>{std::move(value), std::move(rev_args)};
        }
      }, std::move(local));
      if(ret) return *ret;
    }
  }
  template<class primitive_type>
  struct reduction_result {
    expression<primitive_type> head;
    std::vector<expression<primitive_type> > rev_args;
    bool done = false;
    static reduction_result finished(primitive_type head, std::vector<expression<primitive_type> > rev_args) {
      return {value(std::move(head)), std::move(rev_args), true };
    }
  };
  struct trivial_reducer_t {
    template<class primitive_type>
    reduction_result<primitive_type> operator()(primitive_type v, std::vector<expression<primitive_type> > rev_args) const {
      return reduction_result<primitive_type>::finished(std::move(v), std::move(rev_args));
    }
  };
  constexpr trivial_reducer_t trivial_reducer{};
  template<class reducer_t, class primitive_type>
  concept reduction_rule = requires(reducer_t const& r, primitive_type v, std::vector<expression<primitive_type> > vec) {
    {r(std::move(v), std::move(vec))} -> std::convertible_to<reduction_result<primitive_type> >;
  };
  template<class primitive_type, reduction_rule<primitive_type> reducer_t>
  beta_free_expression<primitive_type> reduce_head_with_reducer(reducer_t const& reducer, expression<primitive_type> e, std::vector<expression<primitive_type> > rev_args = {}) {
    while(true) {
      auto state = reduce_to_beta_free_head(std::move(e), std::move(rev_args));
      if(std::holds_alternative<abstract_t<primitive_type> >(state)
        || std::holds_alternative<argument_t<primitive_type> >(std::get<beta_free_application<primitive_type> >(state).head))
        return state; //no more reduction possible because of lambda or argument head
      auto rep = std::get<beta_free_application<primitive_type> >(std::move(state));
      auto head = std::get<value_t<primitive_type> >(std::move(rep.head));
      auto r_args = std::move(rep.rev_args);
      reduction_result<primitive_type> result = reducer(std::move(head.value), std::move(r_args));
      if(result.done) {
        return reduce_to_beta_free_head(std::move(result.head), std::move(result.rev_args));
      } else {
        e = std::move(result.head);
        rev_args = std::move(result.rev_args);
      }
    }
  }
  template<class primitive_type>
  expression<primitive_type> fold_beta_free_expression(beta_free_expression<primitive_type> expr) {
    return std::visit(utility::overloaded{
      [](beta_free_application<primitive_type> e) {
        return std::move(e).fold();
      },
      [](abstract_t<primitive_type> abstract) {
        return (expression<primitive_type>)std::make_shared<abstract_t<primitive_type> >(std::move(abstract));
      }
    }, std::move(expr));
  }
  template<class primitive_type, reduction_rule<primitive_type> reducer_t>
  expression<primitive_type> reduce_globally(reducer_t const& reducer, expression<primitive_type> e) {
    return std::visit(utility::overloaded{
      [&](beta_free_application<primitive_type> e) {
        for(auto& expr: e.rev_args) {
          expr = reduce_globally(reducer, std::move(expr));
        }
        return std::move(e).fold();
      },
      [&](abstract_t<primitive_type> abstract) {
        auto reduced = abstract_t<primitive_type>{reduce_globally(reducer, abstract.body)};
        auto reduced_body = remove_trivial_abstractions(reduced);
        if(reduced_body) return *reduced_body;
        return (expression<primitive_type>)std::make_shared<abstract_t<primitive_type> >(std::move(reduced));
      }
    }, reduce_head_with_reducer(reducer, e));
  }
  template<class primitive_type>
  bool trivial_compare(expression<primitive_type> a, expression<primitive_type> b) {
    auto local_a = a->reduce_to_local_expression();
    auto local_b = b->reduce_to_local_expression();
    if(local_a.index() != local_b.index()) return false;
    if(local_a.index() == 0) {
      auto value_a = std::get<0>(local_a);
      auto value_b = std::get<0>(local_b);
      return trivial_compare(value_a.f, value_b.f) && trivial_compare(value_a.x, value_b.x);
    }
    if(local_a.index() == 1) {
      auto value_a = std::get<1>(local_a);
      auto value_b = std::get<1>(local_b);
      return trivial_compare(value_a.body, value_b.body);
    }

    if(local_a.index() == 2) {
      auto value_a = std::get<2>(local_a);
      auto value_b = std::get<2>(local_b);
      return value_a.index == value_b.index;
    }

    if(local_a.index() == 3) {
      auto value_a = std::get<3>(local_a);
      auto value_b = std::get<3>(local_b);
      return value_a.value == value_b.value;
    }
    std::terminate();
  }
  template<class primitive_type, reduction_rule<primitive_type> reducer_t>
  bool deep_compare(reducer_t const& reducer, expression<primitive_type> a, expression<primitive_type> b) {
    return trivial_compare(reduce_globally(reducer, a), reduce_globally(reducer, b));
  }
}

#endif
