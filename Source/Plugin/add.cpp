#include "../Pipeline/standard_compiler_context.hpp"
#include "../Utility/vector_utility.hpp"
#include <iostream>

extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  std::cout << "Running shared library!!!!\n";
  context->context.rule_collector.add_rule({
    .pattern = {
      .head = context->arena.copy(import_start[0].value),
      .body = {
        .args_captured = 2,
        .steps = mdb::make_vector<new_expression::PatternStep>(
          new_expression::PullArgument{},
          new_expression::DataCheck{
            .capture_index = 0,
            .expected_type = context->u64->type_index
          },
          new_expression::PullArgument{},
          new_expression::DataCheck{
            .capture_index = 1,
            .expected_type = context->u64->type_index
          }
        )
      }
    },
    .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
      [context](std::span<new_expression::WeakExpression> inputs) {
        return context->u64->make_expression(
          context->u64->read_data(context->arena.get_data(inputs[0])) + context->u64->read_data(context->arena.get_data(inputs[1]))
        );
      }
    }
  });
  context->context.rule_collector.add_rule({
    .pattern = {
      .head = context->arena.copy(import_start[1].value),
      .body = {
        .args_captured = 2,
        .steps = mdb::make_vector<new_expression::PatternStep>(
          new_expression::PullArgument{},
          new_expression::DataCheck{
            .capture_index = 0,
            .expected_type = context->u64->type_index
          },
          new_expression::PullArgument{},
          new_expression::DataCheck{
            .capture_index = 1,
            .expected_type = context->u64->type_index
          }
        )
      }
    },
    .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
      [context](std::span<new_expression::WeakExpression> inputs) {
        return context->u64->make_expression(
          context->u64->read_data(context->arena.get_data(inputs[0])) * context->u64->read_data(context->arena.get_data(inputs[1]))
        );
      }
    }
  });
}
