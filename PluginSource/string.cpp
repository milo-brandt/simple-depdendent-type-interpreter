#include "../Source/Plugin/plugin.hpp"

struct Witness{};

extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  auto const& [
    witness,
    yes,
    no,
    len,
    substr,
    starts_with,
    starts_with_len_lte
  ] = mdb::map_span<7>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });

  auto rule_builder = plugin::get_rule_builder(
    context,
    plugin::BoolHandler{
      yes,
      no
    },
    context->u64,
    context->str,
    plugin::ignore,
    plugin::expression_handler,
    plugin::ExactHandler<Witness>{
      context->arena,
      witness
    }
  );
  auto& arena = context->arena;

  rule_builder(len, [](primitive::StringHolder const& str) {
    return str.size();
  });
  rule_builder(substr, [](primitive::StringHolder const& str, std::uint64_t beg, std::uint64_t end, Witness, Witness) {
    return str.substr(beg, end - beg);
  });
  rule_builder(starts_with, [](primitive::StringHolder const& prefix, primitive::StringHolder const& str) {
    return str.get_string().starts_with(prefix.get_string());
  });
  rule_builder(starts_with_len_lte, [witness](primitive::StringHolder const&, primitive::StringHolder const&, Witness) {
    return witness;
  });
}
