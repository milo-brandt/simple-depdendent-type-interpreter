#include "common_routines.hpp"

namespace solver::routine {
  mdb::Future<EquationResult> cast(CastInfo cast) {
    return cast.handle_equation({
      .lhs = std::move(cast.source_type),
      .rhs = std::move(cast.target_type),
      .stack = cast.stack
    }).then([
      depth = cast.stack.depth(),
      variable = std::move(cast.variable),
      source = std::move(cast.source),
      &arena = cast.arena,
      add_rule = std::move(cast.add_rule)
    ](EquationResult result) mutable {
      if(std::holds_alternative<EquationSolved>(result)) {
        add_rule({
          .pattern = new_expression::lambda_pattern(std::move(variable), depth),
          .replacement = std::move(source)
        });
      } else {
        destroy_from_arena(arena, variable, source);
      }
      return result;
    });
  }
  mdb::Future<FunctionCastResult> function_cast(FunctionCastInfo cast) {
    auto [promise, future] = mdb::create_promise_future_pair<FunctionCastResult>();
    auto is_function = cast.handle_equation({
      .lhs = std::move(cast.function_type),
      .rhs = std::move(cast.expected_function_type),
      .stack = cast.stack
    });
    std::move(is_function).listen([
      &arena = cast.arena,
      add_rule = std::move(cast.add_rule),
      handle_equation = std::move(cast.handle_equation),
      function_variable = std::move(cast.function_variable),
      argument_variable = std::move(cast.argument_variable),
      function_value = std::move(cast.function_value),
      argument_value = std::move(cast.argument_value),
      argument_type = std::move(cast.argument_type),
      expected_argument_type = std::move(cast.expected_argument_type),
      stack = std::move(cast.stack),
      promise = std::move(promise)
    ](EquationResult result) mutable {
      if(std::holds_alternative<EquationSolved>(result)) {
        add_rule({
          .pattern = new_expression::lambda_pattern(std::move(function_variable), stack.depth()),
          .replacement = std::move(function_value)
        });
        handle_equation({
          .lhs = std::move(argument_type),
          .rhs = std::move(expected_argument_type),
          .stack = stack
        }).listen([
          &arena,
          argument_variable = std::move(argument_variable),
          argument_value = std::move(argument_value),
          stack = std::move(stack),
          promise = std::move(promise),
          add_rule = std::move(add_rule)
        ](EquationResult result) mutable {
          if(std::holds_alternative<EquationSolved>(result)) {
            add_rule({
              .pattern = new_expression::lambda_pattern(std::move(argument_variable), stack.depth()),
              .replacement = std::move(argument_value)
            });
          } else {
            destroy_from_arena(arena, argument_variable, argument_value);
          }
          promise.set_value({
            .result = std::move(result),
            .lhs_was_function = true
          });
        });
      } else {
        destroy_from_arena(arena, function_variable, argument_variable, function_value,
          argument_value, argument_type, expected_argument_type);
        promise.set_value({
          .result = std::move(result),
          .lhs_was_function = false
        });
      }
    });
    return std::move(future);
  }
}
