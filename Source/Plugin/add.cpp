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
  auto const& handler_for() {
    if constexpr(index < sizeof...(Handlers)) {
      auto const& handler = std::get<index>(handlers);
      using ReadType = decltype(handler->read_data(std::declval<new_expression::Data>()));
      if constexpr(std::is_same_v<std::decay_t<ReadType>, T>) {
        return handler;
      } else {
        return handler_for<T, index + 1>();
      }
    } else {
      static_assert(index < sizeof...(Handlers), "Handler not found for type.");
    }
  }
  template<class Callback>
  void operator()(new_expression::WeakExpression head, Callback callback) {
    [&]<class Ret, class... Args>(mdb::FunctionInfo<Ret(Args...)>) {
      return simple_pattern_builder(context)(head, handler_for<Args>()...) >> [callback = std::move(callback), ret_handler = handler_for<Ret>()]<class... Passed>(Passed&&... passed) {
        return ret_handler->make_expression(callback(std::forward<Passed>(passed)...));
      };
    }(mdb::FunctionInfoFor<Callback>{});
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
  auto const& [add, sub, mul, idiv] = map_span<4>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });
  auto rule_builder = get_rule_builder(context, context->u64);
  rule_builder(add, [](std::uint64_t x, std::uint64_t y) {
    return x + y;
  });
  rule_builder(sub, [](std::uint64_t x, std::uint64_t y) {
    return x - y;
  });
  rule_builder(mul, [](std::uint64_t x, std::uint64_t y) {
    return x * y;
  });
  rule_builder(idiv, [](std::uint64_t x, std::uint64_t y) {
    return x / y;
  });
}
