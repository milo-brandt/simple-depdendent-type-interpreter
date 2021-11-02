/*

#include "../Source/Pipeline/standard_compiler_context.hpp"
#include "../Source/Utility/vector_utility.hpp"

extern "C" void initialize(new_expression::Arena* arena, solver::BasicContext* context, new_expression::OwnedExpression* import_start, new_expression::OwnedExpression* import_end) {
  context->rule_collector.add_rule({
    .pattern = {
      .head = arena->copy(import_start[0]),
      .body = {
        .args_captured = 2,
        .steps = mdb::make_vector<new_expression::PatternStep>(
          new_expression::PullArgument{},
          new_expression::DataCheck{
            .capture_index = 0,
            .expected_type = environment.u64()->type_index
          },
          new_expression::PullArgument{},
          new_expression::DataCheck{
            .capture_index = 1,
            .expected_type = environment.u64()->type_index
          }
        )
      }
    },
    .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
      [&](std::span<new_expression::WeakExpression> inputs) {
        return environment.u64()->make_expression(
            environment.u64()->read_data(arena.get_data(inputs[0])) * environment.u64()->read_data(arena.get_data(inputs[1]))
        );
      }
    }
  });
}

*/
