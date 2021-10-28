#ifndef MODULE_STORE_HPP
#define MODULE_STORE_HPP

#include "core.hpp"
#include "../NewExpression/arena.hpp"
#include "../NewExpression/type_utility.hpp"
#include "../NewExpression/rule.hpp"

namespace expr_module {
  struct StoreContext {
    new_expression::Arena& arena;
    new_expression::RuleCollector& rule_collector;
    new_expression::PrimitiveTypeCollector& type_collector;
    mdb::function<std::optional<std::uint32_t>(new_expression::WeakExpression)> get_import_index_of;
    mdb::function<std::uint32_t()> get_import_size;
  };
  struct StoreInfo {
    std::vector<new_expression::OwnedExpression> exports;
  };
  //Note: can't handle function replacements yet
  Core store(StoreContext, StoreInfo);
}

#endif
