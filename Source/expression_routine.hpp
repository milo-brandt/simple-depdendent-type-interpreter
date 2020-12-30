#include "expressions.hpp"
#include "lifted_state_machine.hpp"
#include "fn_once.hpp"
#include <optional>

#include <iostream>

namespace expressionz::coro {
  namespace handles {
    struct expression {
      std::size_t index;
      expression() = delete;
      explicit expression(std::size_t index):index(index){}
    };
    struct apply {
      expression f;
      expression x;
    };
    struct abstract {
      expression body;
    };
    struct argument {
      std::size_t index;
    };
    template<class T>
    using expanded = std::variant<T, apply, abstract, argument>;
  };
  namespace actions {
    struct expand {
      handles::expression term;
      expand(handles::expression term):term(std::move(term)){}
    };
    template<class T>
    struct replace {
      handles::expression term;
      expression<T> value;
      replace(handles::expression term, expression<T> value):term(std::move(term)), value(std::move(value)){}
    };
    template<class T>
    replace(handles::expression, expression<T>) -> replace<T>;
    struct collapse {
      handles::expression term;
      collapse(handles::expression term):term(std::move(term)){}
    };
  }
  namespace external_states {
    template<class T, class R>
    struct expand;
    template<class T, class R>
    struct replace;
    template<class T, class R>
    struct collapse;
    struct failed_t{};
    constexpr failed_t failed{};
    template<class T, class R>
    using any = std::variant<R, failed_t, expand<T, R>, replace<T, R>, collapse<T, R> >;
    template<class T, class R>
    struct expand {
      handles::expression term;
      utility::fn_once<any<T,R>(handles::expanded<T>)> resume;
    };
    template<class T, class R>
    struct replace {
      handles::expression term;
      expression<T> value;
      utility::fn_once<any<T,R>()> resume;
    };
    template<class T, class R>
    struct collapse {
      handles::expression term;
      utility::fn_once<any<T,R>(expression<T>)> resume;
    };
  };
  template<class T, class R>
  struct expression_routine_definition;
  template<class T, class R>
  using expression_routine = lifted_state_machine::coro_type<expression_routine_definition<T, R> >;
  template<class T, class R>
  struct expression_routine_definition : lifted_state_machine::coro_base<expression_routine_definition<T, R> > {
    using coro_base = lifted_state_machine::coro_base<expression_routine_definition<T, R> >;
    template<class V = void>
    using resume_handle = typename coro_base::template resume_handle<V>;
    using waiting_handle = typename coro_base::waiting_handle;
    using returning_handle = typename coro_base::returning_handle;
    struct state{};
    using paused_type = external_states::any<T, R>;
    using starter_base = resume_handle<>;
    static starter_base create_object(resume_handle<> handle) {
      return handle;
    }
    template<class S> //NOTE: Might *call* the resume handle - not safe to call while coroutine is running
    static paused_type wrap_inner_return(resume_handle<S> resume, external_states::any<T, S> inner) {
      if(S* ret = std::get_if<0>(&inner)) {
        return std::move(resume).resume(std::move(*ret));
      } else if(inner.index() == 1) {
        return external_states::failed;
      } else if(external_states::expand<T, S>* expand = std::get_if<2>(&inner)) {
        return external_states::expand<T, R>{
          std::move(expand->term),
          [inner_resume = std::move(expand->resume), outer_resume = std::move(resume)](handles::expanded<T> e) mutable{
            return wrap_inner_return(std::move(outer_resume), std::move(inner_resume)(std::move(e)));
          }
        };
      } else if(external_states::replace<T, S>* replace = std::get_if<3>(&inner)) {
        return external_states::replace<T, R>{
          std::move(replace->term),
          std::move(replace->value),
          [inner_resume = std::move(replace->resume), outer_resume = std::move(resume)]() mutable{
            return wrap_inner_return(std::move(outer_resume), std::move(inner_resume)());
          }
        };
      } else if(external_states::collapse<T, S>* collapse = std::get_if<4>(&inner)) {
        return external_states::collapse<T, R>{
          std::move(collapse->term),
          [inner_resume = std::move(collapse->resume), outer_resume = std::move(resume)](expression<T> e) mutable{
            return wrap_inner_return(std::move(outer_resume), std::move(inner_resume)(std::move(e)));
          }
        };
      }
      std::terminate(); //unreachable
    }
    template<class S>
    using any_awaiter = ::coro::variant_awaiter<S, lifted_state_machine::immediate_awaiter<S>, lifted_state_machine::await_from_pointer<S>, lifted_state_machine::await_with_no_resume<S> >;
    template<class S>
    static any_awaiter<S> on_await(expression_routine<T, S> routine, state&, waiting_handle&& handle) {
      auto result = std::move(routine).resume();
      if(S* ret = std::get_if<0>(&result)) { //cases without suspension need to be handled separately
        return std::move(handle).immediate_result(std::move(*ret));
      } else if(result.index() == 1) {
        return std::move(handle).template destroying_result<S>(external_states::failed);
      } else {
        return std::move(handle).template suspending_result<S>([&](auto resume){ return wrap_inner_return(std::move(resume), std::move(result)); });
      }
    }
    template<class S>
    static auto on_yield(expression_routine<T, S> routine, state&, waiting_handle&& handle) {
    return std::move(handle).template destroying_result(std::move(routine).resume());
    }
    static auto on_await(actions::expand expand, state&, waiting_handle&& handle) {
      return std::move(handle).template suspending_result<handles::expanded<T> >([&](auto resume){
        return external_states::expand<T, R>{
          .term = std::move(expand.term),
          .resume = [r = std::move(resume)](auto expanded) mutable{ return std::move(r).resume(std::move(expanded)); }
        };
      });
    }
    static auto on_await(actions::replace<T> replace, state&, waiting_handle&& handle) {
      return std::move(handle).suspending_result([&](auto resume){
        return external_states::replace<T, R>{
          .term = std::move(replace.term),
          .value = std::move(replace.value),
          .resume = [r = std::move(resume)]() mutable{ return std::move(r).resume(); }
        };
      });
    }
    static auto on_await(actions::collapse collapse, state&, waiting_handle&& handle) {
      return std::move(handle).template suspending_result<expression<T> >([&](auto resume){
        return external_states::collapse<T, R>{
          .term = std::move(collapse.term),
          .resume = [r = std::move(resume)](auto collapsed) mutable{ return std::move(r).resume(std::move(collapsed)); }
        };
      });
    }
    static void on_return_value(R value, state&, returning_handle&& handle) {
      return std::move(handle).result(std::move(value));
    }
  };

  template<class T>
  struct simplification_state {
      using entry = std::variant<expression<T>, handles::apply, handles::abstract, handles::argument, T>;
      std::vector<entry> entries;
      handles::expanded<T> expand(handles::expression e) {
        assert(e.index < entries.size());
        if(auto* expr = std::get_if<0>(&entries[e.index])) {
          auto local_expr = expr->local_form();
          if(auto* val = get_if<local_type::value>(&local_expr)) {
            auto ret = *val;
            entries[e.index] = ret;
            return std::move(ret);
          } else if(auto* apply = get_if<local_type::apply>(&local_expr)) {
            auto base = entries.size();
            entries.push_back(apply->f);
            entries.push_back(apply->x);
            auto ret = handles::apply{ handles::expression{base}, handles::expression{base+1} };
            entries[e.index] = ret;
            return ret;
          } else if(auto* abstract = get_if<local_type::abstract>(&local_expr)) {
            auto base = entries.size();
            entries.push_back(abstract->body);
            auto ret = handles::abstract{handles::expression{base}};
            entries[e.index] = ret;
            return ret;
          } else {
            auto& argument = get<local_type::argument>(local_expr);
            auto ret = handles::argument{argument.index};
            entries[e.index] = ret;
            return ret;
          }
        } else if(auto* app = std::get_if<1>(&entries[e.index])) {
          return *app;
        } else if(auto* ab = std::get_if<2>(&entries[e.index])) {
          return *ab;
        } else if(auto* arg = std::get_if<3>(&entries[e.index])) {
          return *arg;
        } else if(auto* val = std::get_if<4>(&entries[e.index])) {
          return *val;
        }
        std::terminate();
      }
      void replace(handles::expression e, expression<T> expr) {
        entries[e.index] = expr;
      }
      expression<T> collapse(handles::expression e) {
        if(auto* expr = std::get_if<0>(&entries[e.index])) {
          return *expr;
        } else if(auto* app = std::get_if<1>(&entries[e.index])) {
          expression<T> ret = apply(collapse(app->f), collapse(app->x));
          entries[e.index] = ret;
          return ret;
        } else if(auto* ab = std::get_if<2>(&entries[e.index])) {
          expression<T> ret = abstract(collapse(ab->body));
          entries[e.index] = ret;
          return ret;
        } else if(auto* arg = std::get_if<3>(&entries[e.index])) {
          expression<T> ret = argument(arg->index);
          entries[e.index] = ret;
          return ret;
        } else if(auto* val = std::get_if<4>(&entries[e.index])) {
          expression<T> ret = *val;
          entries[e.index] = ret;
          return ret;
        }
        std::terminate();
      }
      handles::expression push(expression<T> expr) {
        auto base = entries.size();
        entries.push_back(expr);
        return handles::expression{base};
      }
  };
  template<class T, class R, class L>
  std::optional<R> run_routine_on_state(simplification_state<T>& expr_state, expression_routine<T, R> routine, L&& logger = [](auto&&...){}) {
    auto state = std::move(routine).resume();
    while(state.index() > 1) {
      if(auto* expand = std::get_if<external_states::expand<T, R> >(&state)) {
        state = std::move(*expand).resume(expr_state.expand(expand->term));
      } else if(auto* replace = std::get_if<external_states::replace<T, R> >(&state)) {
        expr_state.replace(replace->term, replace->value);
        logger(replace->term);
        state = std::move(*replace).resume();
      } else if(auto* collapse = std::get_if<external_states::collapse<T, R> >(&state)) {
        state = std::move(*collapse).resume(expr_state.collapse(collapse->term));
      } else {
        std::terminate();
      }
    }
    if(auto* val = std::get_if<0>(&state)) {
      return std::move(*val);
    } else {
      return std::nullopt;
    }
  }
  template<class F, class T, class... Args>
  auto run_routine(F f, expression<T> expr, Args&&... args) {
    auto routine = std::move(f)(handles::expression{0}, std::forward<Args>(args)...);
    simplification_state<T> expr_state;
    expr_state.entries.push_back(expr);
    auto r = run_routine_on_state(expr_state, std::move(routine), [](auto&&...){});
    return std::make_pair(expr_state.collapse(handles::expression{0}), std::move(r));
  }
  template<class L, class F, class T, class... Args>
  auto run_routine_with_logger_routine(L logger, F f, expression<T> expr, Args&&... args) {
    auto outer_handle = handles::expression{0};
    auto routine = std::move(f)(outer_handle, std::forward<Args>(args)...);
    simplification_state<T> expr_state;
    expr_state.entries.push_back(expr);
    auto r = run_routine_on_state(expr_state, std::move(routine), [&](handles::expression position){
      auto executor = [&]<class R>(R&& what) {
        run_routine_on_state(expr_state, std::forward<R>(what), [](auto&&...){});
      };
      logger(executor, outer_handle, position);
    });
    return std::make_pair(expr_state.collapse(handles::expression{0}), std::move(r));
  }
}
