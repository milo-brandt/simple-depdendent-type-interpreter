#include "c_vector.hpp"

struct Witness{};

extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  auto const& [
    native_empty_vec,
    native_cons_vec,
    witness,
    c_vector,
    empty_vec,
    cons_vec,
    c_vector_to_vector,
    c_vec_len,
    c_vec_at
  ] = mdb::map_span<9>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });

  auto vec = VecData::register_on(context->arena, context->arena.copy(c_vector));

  auto rule_builder = plugin::get_rule_builder(
    context,
    context->u64,
    plugin::ignore,
    plugin::expression_handler,
    vec,
    plugin::ExactHandler<Witness>{
      context->arena,
      witness
    }
  );
  auto& arena = context->arena;

  rule_builder(empty_vec, [](new_expression::OwnedExpression type) {
    return VecStore{
      .entry_type = std::move(type),
      .begin = nullptr,
      .end = nullptr
    };
  });
  rule_builder(cons_vec, [&arena](new_expression::OwnedExpression type, new_expression::OwnedExpression head, VecStore const& tail) {
    auto old_extent = tail.end - tail.begin;
    auto new_allocation = (new_expression::OwnedExpression*)malloc(sizeof(new_expression::OwnedExpression) * (1 + old_extent));
    new (new_allocation) new_expression::OwnedExpression{std::move(head)};
    for(std::size_t i = 0; i < old_extent; ++i) {
      new (new_allocation + i) new_expression::OwnedExpression{arena.copy(tail.begin[i])};
    }
    return VecStore{
      .entry_type = std::move(type),
      .begin = new_allocation,
      .end = new_allocation + (1 + old_extent)
    };
  });
  rule_builder(c_vector_to_vector, [&arena, native_empty_vec, native_cons_vec](new_expression::WeakExpression type, VecStore const& vec) {
    auto ret = arena.apply(
      arena.copy(native_empty_vec),
      arena.copy(type)
    );
    auto it = vec.end;
    while(it != vec.begin) {
      --it;
      ret = arena.apply(
        arena.copy(native_cons_vec),
        arena.copy(type),
        arena.copy(*it),
        std::move(ret)
      );
    }
    return ret;
  });
  rule_builder(c_vec_len, [](plugin::Ignore, VecStore const& vec) {
    return (std::uint64_t)(vec.end - vec.begin);
  });
  rule_builder(c_vec_at, [](plugin::Ignore, VecStore const& vec, std::uint64_t index, Witness) -> new_expression::WeakExpression {
    return vec.begin[index];
  });
  context->plugin_data.insert(std::make_pair("vector_info", new new_expression::SharedDataTypePointer<VecData>(std::move(vec))));
}
extern "C" void cleanup(pipeline::compile::StandardCompilerContext* context) {
  delete (new_expression::SharedDataTypePointer<VecData>*)context->plugin_data.at("vector_info");
}
