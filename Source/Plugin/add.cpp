#include "../Pipeline/standard_compiler_context.hpp"
#include "../Utility/vector_utility.hpp"
#include <iostream>
#include <type_traits>
#include "../Utility/function_info.hpp"

/*

*/


template<std::size_t n, class T, class Map, class R = std::invoke_result_t<Map, T const&> >
std::array<R, n> map_span(T* start, T* end, Map&& map) {
  if(end - start != n) std::terminate();
  return [&]<std::size_t... is>(std::index_sequence<is...>) {
    return std::array<R, n>{
      map(start[is])...
    };
  }(std::make_index_sequence<n>{});
}
template<std::size_t n, class T, class Map, class R = std::invoke_result_t<Map, T const&> >
std::array<R, n> map_span(std::span<T> span, Map&& map) {
  return destructure_span<n>(span.begin(), span.end(), std::forward<Map>(map));
}
template<class... Args>
struct SimplePattern {
  pipeline::compile::StandardCompilerContext& context;
  new_expression::WeakExpression head;
  std::tuple<Args...> args;
};
template<class T>
void add_constraints_for(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, new_expression::SharedDataTypePointer<T> const& ptr) {
  steps.push_back(new_expression::DataCheck{
    .capture_index = capture_index,
    .expected_type = ptr->type_index
  });
}
template<class T>
decltype(auto) read_arg(new_expression::Arena& arena, new_expression::WeakExpression expr, new_expression::SharedDataTypePointer<T> const& ptr) {
  return ptr->read_data(arena.get_data(expr));
}

namespace builder {
  template<class Final, class F1, class... Fs>
  decltype(auto) call_getters(Final&& final, F1&& f1, Fs&&... fs) { //f_n(g) should call g(x1, x2, x3) for some set of args.
    if constexpr(sizeof...(Fs) == 0) {
      return f1(std::forward<Final>(final));
    } else {
      return call_getters([&]<class... Args>(Args&&... args){
        return f1([&]<class... F1Args>(F1Args&&... f1_args) {
          return final(std::forward<F1Args>(f1_args)..., std::forward<Args>(args)...);
        });
      }, std::forward<Fs>(fs)...);
    }
  }
  template<class Final>
  decltype(auto) ordered_eval(Final&& final) {
    return final();
  }
  template<class Final, class F1, class... Fs>
  decltype(auto) ordered_eval(Final&& final, F1&& f1, Fs&&... fs) {
    return ordered_eval([&, arg = f1()]<class... Rest>(Rest&&... rest) mutable -> decltype(auto) {
      return final(std::forward<decltype(f1())>(arg), std::forward<Rest>(rest)...);
    }, std::forward<Fs>(fs)...);
  }


  struct Ignore{};
  constexpr Ignore ignore{};

  struct ExpressionHandler{};
  constexpr ExpressionHandler expression_handler{};

  template<class R> requires std::is_same_v<std::decay_t<R>, Ignore>
  auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, Ignore) {
    return [](new_expression::Arena& arena, std::span<new_expression::WeakExpression> const& args, auto&& callback) {
      return callback(ignore);
    };
  };
  template<class R> requires std::is_same_v<R, new_expression::WeakExpression>
  auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, ExpressionHandler) {
    return [capture_index](new_expression::Arena& arena, std::span<new_expression::WeakExpression> const& args, auto&& callback) {
      return callback(args[capture_index]);
    };
  };

  template<class R, class T>
  auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, new_expression::SharedDataTypePointer<T> ptr)
    requires requires{ (R)ptr->read_data(std::declval<new_expression::Data>()); }
  {
    steps.push_back(new_expression::DataCheck{
      .capture_index = capture_index,
      .expected_type = ptr->type_index
    });
    return [capture_index, ptr = std::move(ptr)](new_expression::Arena& arena, std::span<new_expression::WeakExpression> const& args, auto&& callback) -> decltype(auto) {
      return callback(ptr->read_data(arena.get_data(args[capture_index])));
    };
  }
  template<class R, class T>
  auto get_embedder(new_expression::SharedDataTypePointer<T> ptr)
    requires requires{ ptr->make_expression(std::declval<R>()); }
  {
    return [ptr = std::move(ptr)](new_expression::Arena& arena, R value) {
      return ptr->make_expression(std::move(value));
    };
  };
  template<class R> requires std::is_same_v<std::decay_t<R>, new_expression::WeakExpression>
  auto get_embedder(ExpressionHandler) {
    return [](new_expression::Arena& arena, R value) {
      return arena.copy(value);
    };
  };
  template<class R> requires std::is_same_v<R, new_expression::OwnedExpression>
  auto get_embedder(ExpressionHandler) {
    return [](new_expression::Arena& arena, R value) {
      return std::move(value);
    };
  };
  template<class R> requires std::is_same_v<R, new_expression::OwnedExpression&&>
  auto get_embedder(ExpressionHandler) {
    return [](new_expression::Arena& arena, R value) {
      return std::move(value);
    };
  };
  template<class R> requires std::is_same_v<R, new_expression::OwnedExpression const&>
  auto get_embedder(ExpressionHandler) {
    return [](new_expression::Arena& arena, R value) {
      return arena.copy(value);
    };
  };


}

/*

*/


auto simple_pattern_builder(pipeline::compile::StandardCompilerContext* context) {
  return [context]<class... Args>(new_expression::WeakExpression head, Args... args) {
    return SimplePattern<Args...>{
      *context,
      head,
      std::make_tuple(std::move(args)...)
    };
  };
}
template<class Callback, class... Args>
void operator>>(SimplePattern<Args...> pattern, Callback callback) {
  auto& arena = pattern.context.arena;
  pattern.context.context.rule_collector.add_rule({
    .pattern = {
      .head = arena.copy(pattern.head),
      .body = {
        .args_captured = sizeof...(Args),
        .steps = [&] {
          std::vector<new_expression::PatternStep> steps;
          for(std::size_t i = 0; i < sizeof...(Args); ++i) {
            steps.push_back(new_expression::PullArgument{});
          }
          [&]<std::size_t... is>(std::index_sequence<is...>) {
            (add_constraints_for(is, steps, std::get<is>(pattern.args)) , ...);
          }(std::make_index_sequence<sizeof...(Args)>{});
          return steps;
        }()
      }
    },
    .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
      [&context = pattern.context, callback = std::move(callback), args = pattern.args](std::span<new_expression::WeakExpression> inputs) {
        if(inputs.size() != sizeof...(Args)) std::terminate();
        return [&]<std::size_t... is>(std::index_sequence<is...>) {
          return callback(read_arg(context.arena, inputs[is], std::get<is>(args))...);
        }(std::make_index_sequence<sizeof...(Args)>{});
      }
    }
  });
}

template<class... Handlers>
struct RuleBuilder {
  pipeline::compile::StandardCompilerContext* context;
  std::tuple<Handlers...> handlers;
  template<class T, std::size_t index = 0>
  auto pull_argument(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps) {
    if constexpr(index < sizeof...(Handlers)) {
      auto const& handler = std::get<index>(handlers);
      if constexpr(requires{ builder::add_constraints_and_get_arg_puller<T>(capture_index, steps, handler); }) {
        return builder::add_constraints_and_get_arg_puller<T>(capture_index, steps, handler);
      } else {
        return pull_argument<T, index + 1>(capture_index, steps);
      }
    } else {
      static_assert(index < sizeof...(Handlers), "Handler not found for type.");
    }
  }
  template<class T, std::size_t index = 0>
  auto embedder_for() {
    if constexpr(index < sizeof...(Handlers)) {
      auto const& handler = std::get<index>(handlers);
      if constexpr(requires{ builder::get_embedder<T>(handler); }) {
        return builder::get_embedder<T>(handler);
      } else {
        return embedder_for<T, index + 1>();
      }
    } else {
      static_assert(index < sizeof...(Handlers), "Handler not found for type.");
    }
  }

  template<class Callback>
  void operator()(new_expression::WeakExpression head, Callback callback) {
    [&]<class Ret, class... Args>(mdb::FunctionInfo<Ret(Args...)>) {
      std::vector<new_expression::PatternStep> steps;
      std::size_t capture_count = 0;
      auto arg_puller_tuple = builder::ordered_eval([&]<class... ArgPuller>(ArgPuller&&... arg_pullers) {
        return std::make_tuple(std::move(arg_pullers)...);
      }, [&] {
        steps.push_back(new_expression::PullArgument{});
        return pull_argument<Args>(capture_count++, steps);
      }...);
      auto& arena = context->arena;
      context->context.rule_collector.add_rule({
        .pattern = {
          .head = arena.copy(head),
          .body = {
            .args_captured = sizeof...(Args),
            .steps = std::move(steps)
          }
        },
        .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
          [capture_count, &context = *context, callback = std::move(callback), arg_puller_tuple = std::move(arg_puller_tuple), embedder = embedder_for<Ret>()](std::span<new_expression::WeakExpression> inputs) {
            if(inputs.size() != capture_count) std::terminate();
            return embedder(context.arena, std::apply([&](auto const&... getters) { return builder::call_getters(callback, [&](auto&& callback) { return getters(context.arena, inputs, callback); }...); }, arg_puller_tuple));
          }
        }
      });
    }(mdb::FunctionInfoFor<Callback>());
  }
};
template<class... Handlers>
RuleBuilder<Handlers...> get_rule_builder(pipeline::compile::StandardCompilerContext* context, Handlers... handlers) {
  return {
    .context = context,
    .handlers = std::make_tuple(std::move(handlers)...)
  };
}

/*
  Better: bind(add, [](std::uint64_t x, std::uint64_t y) {
    return x + y;
  });
*/


extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  auto const& [zero, succ, add, sub, to_nat] = map_span<5>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });
  auto rule_builder = get_rule_builder(context, context->u64, builder::ignore, builder::expression_handler);
  auto& arena = context->arena;
  rule_builder(add, [](std::uint64_t x, std::uint64_t y) {
    return x + y;
  });
  rule_builder(sub, [](std::uint64_t x, std::uint64_t y) {
    return x - y;
  });
  rule_builder(to_nat, [&arena, zero, succ, to_nat, u64 = context->u64](std::uint64_t x) {
    if(x == 0) {
      return arena.copy(zero);
    } else {
      return arena.apply(
        arena.copy(succ),
        arena.apply(
          arena.copy(to_nat),
          u64->make_expression(x - 1)
        )
      );
    }
  });
}
