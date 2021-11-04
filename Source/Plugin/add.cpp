#include "../Pipeline/standard_compiler_context.hpp"
#include "../Utility/vector_utility.hpp"
#include <iostream>
#include <type_traits>

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


extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  auto const& [add, mul] = map_span<2>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });
  auto simple_pattern = simple_pattern_builder(context);

  simple_pattern(add, context->u64, context->u64) >> [context](std::uint64_t x, std::uint64_t y) {
    return context->u64->make_expression(x + y);
  };
  simple_pattern(mul, context->u64, context->u64) >> [context](std::uint64_t x, std::uint64_t y) {
    return context->u64->make_expression(x * y);
  };

}
