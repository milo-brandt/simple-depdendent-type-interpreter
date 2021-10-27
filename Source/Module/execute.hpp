#ifndef MODULE_EXECUTE_HPP
#define MODULE_EXECUTE_HPP

#include "core.hpp"
#include "../NewExpression/arena.hpp"
#include "../NewExpression/on_arena_utility.hpp"
#include "../NewExpression/type_utility.hpp"
#include "../NewExpression/rule.hpp"

namespace expr_module {
  struct ExecuteContext {
    new_expression::Arena& arena;
    new_expression::RuleCollector& rule_collector;
    new_expression::PrimitiveTypeCollector& type_collector;
  };
  struct ImportInfo {
    std::vector<new_expression::OwnedExpression> value_imports;
    std::vector<std::uint64_t> data_type_import_indices;
    std::vector<mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)> > c_replacement_imports;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct ModuleResult {
    std::vector<new_expression::OwnedExpression> exports;
    static constexpr auto part_info = mdb::parts::simple<1>;
  };
  ModuleResult execute(ExecuteContext context, Core const& core, ImportInfo imports);
}

#endif
