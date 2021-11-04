#include "../Source/Plugin/plugin.hpp"

struct Witness{};

extern "C" void initialize(pipeline::compile::StandardCompilerContext* context, new_expression::TypedValue* import_start, new_expression::TypedValue* import_end) {
  auto const& [
    zero,
    succ,
    witness,
    yes,
    no,
    eq,
    lt,
    gt,
    add,
    sub,
    mul,
    idiv,
    mod,
    to_nat
  ] = mdb::map_span<14>(import_start, import_end, [&](auto const& v) -> new_expression::WeakExpression { return v.value; });

  auto rule_builder = plugin::get_rule_builder(
    context,
    plugin::BoolHandler{
      context->arena,
      yes,
      no
    },
    context->u64,
    plugin::ignore,
    plugin::expression_handler,
    plugin::ExactHandler<Witness>{
      context->arena,
      witness
    }
  );
  auto& arena = context->arena;

  rule_builder(eq, [](std::uint64_t x, std::uint64_t y) {
    return x == y;
  });
  rule_builder(gt, [](std::uint64_t x, std::uint64_t y) {
    return x > y;
  });
  rule_builder(lt, [](std::uint64_t x, std::uint64_t y) {
    return x < y;
  });
  rule_builder(add, [](std::uint64_t x, std::uint64_t y) {
    return x + y;
  });
  rule_builder(sub, [](std::uint64_t x, std::uint64_t y) {
    return x - y;
  });
  rule_builder(mul, [](std::uint64_t x, std::uint64_t y) {
    return x * y;
  });
  rule_builder(idiv, [](std::uint64_t x, std::uint64_t y, Witness) {
    return x / y;
  });
  rule_builder(mod, [](std::uint64_t x, std::uint64_t y, Witness) {
    return x % y;
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
