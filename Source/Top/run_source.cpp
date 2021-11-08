#include "run_source.hpp"
#include <sstream>

mdb::Result<std::pair<pipeline::compile::EvaluateInfo, ModuleLoadInfo>, std::string> run_source(new_expression::Arena& arena, pipeline::compile::StandardCompilerContext& context, std::string_view source) {
  auto module_info = context.create_module_primitives();
  ModuleLoadInfo load_info;

  auto parsed_module_result = bind(
    pipeline::compile::lex({context.arena, source}),
    [&](auto lexed) {
      return parse_module(std::move(lexed));
    });
  if(parsed_module_result.holds_error()) {
    return parsed_module_result.get_error();
  }
  auto& parsed_module = parsed_module_result.get_value();
  std::unordered_map<std::string, new_expression::TypedValue> names_to_values;
  new_expression::RAIIDestroyer destroyer{arena, names_to_values};
  for(auto const& entry : context.names_to_values) {
    names_to_values.insert(std::make_pair(entry.first, copy_on_arena(context.arena, entry.second)));
  }
  load_info.resolve_imports(parsed_module.first, names_to_values, context, module_info);
  auto compilation_result = map(
    resolve(std::move(parsed_module.second), {
      .names_to_values = names_to_values,
      .u64 = context.u64.get(),
      .str = context.str.get(),
      .empty_vec = new_expression::TypedValue{
        .value = context.arena.copy(context.empty_vec),
        .type = context.arena.copy(context.context.type_collector.get_type_of(context.empty_vec))
      },
      .cons_vec = new_expression::TypedValue{
        .value = context.arena.copy(context.cons_vec),
        .type = context.arena.copy(context.context.type_collector.get_type_of(context.cons_vec))
      }
    }),
    [&](auto resolved) {
      return evaluate(std::move(resolved), {
        .context = context.context
      });
    }
  );
  destroy_from_arena(arena, module_info);
  if(auto* val = compilation_result.get_if_value()) {
    return std::make_pair(
      std::move(*val),
      std::move(load_info)
    );
  } else {
    return compilation_result.get_error();
  }
}
