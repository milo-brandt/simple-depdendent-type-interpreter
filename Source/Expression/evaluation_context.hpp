#ifndef EVALUATION_CONTEXT_HPP
#define EVALUATION_CONTEXT_HPP

#include "expression_tree.hpp"

namespace expression {
  constexpr auto lambda_pattern = [](std::uint64_t head, std::uint64_t args) {
    pattern::Pattern ret = pattern::Fixed{head};
    for(std::uint64_t i = 0; i < args; ++i) {
      ret = pattern::Apply{std::move(ret), pattern::Wildcard{}};
    }
    return ret;
  };
  constexpr auto multi_apply = []<class... Args>(tree::Expression head, Args&&... args) {
    ((head = tree::Apply{std::move(head), std::forward<Args>(args)}) , ...);
    return head;
  };
  struct ExternalInfo {
    bool is_axiom;
    tree::Expression type;
    struct RuleInfo {
      std::uint64_t index;
      std::uint64_t arg_count;
    };
    std::vector<RuleInfo> rules;
    std::vector<RuleInfo> data_rules;
  };
  struct Primitives {
    std::uint64_t type;
    std::uint64_t arrow;
    std::uint64_t type_family; // \T:Type.T -> Type;
    std::uint64_t constant; // \T:Type.\t:T.Type;
    std::uint64_t constant_codomain_1;
    std::uint64_t constant_codomain_2;
    std::uint64_t constant_codomain_3;
    std::uint64_t constant_codomain_4;
    std::uint64_t id; // \T:Type.\t:T.t
    std::uint64_t id_codomain;
    std::uint64_t arrow_codomain;
    std::uint64_t push_vec = -1; //set externally, for now
    std::uint64_t empty_vec = -1;
  };
  struct Context {
    std::vector<Rule> rules;
    std::vector<DataRule> data_rules;
    std::vector<ExternalInfo> external_info;
    Primitives primitives;
    Context();
    TypedValue get_external(std::uint64_t);
    void add_rule(Rule);
    void replace_rule(std::size_t index, Rule new_rule);
    void add_data_rule(DataRule);
    tree::Expression reduce(tree::Expression tree);
    tree::Expression reduce_filter_rules(tree::Expression tree, mdb::function<bool(Rule const&)> filter);
    struct FunctionData {
      tree::Expression domain;
      tree::Expression codomain;
    };
    std::optional<FunctionData> get_domain_and_codomain(tree::Expression);
    template<std::uint64_t count, class Callback> decltype(auto) create_variables(Callback&& callback) {
      bool called_build = false;
      decltype(auto) ret = [&]<std::uint64_t... indices>(std::integer_sequence<std::uint64_t, indices...>) {
        auto begin = external_info.size();
        return std::forward<Callback>(callback)([&](auto&&... externals) {
          static_assert(sizeof...(externals) == count);
          if(called_build || begin != external_info.size()) std::terminate();
          called_build = true;
          (external_info.push_back(std::move(externals)) , ...);
        }, begin + indices...);
      }(std::make_integer_sequence<std::uint64_t, count>{});
      if(!called_build) std::terminate();
      return ret;
    }
    std::uint64_t create_variable(ExternalInfo info) {
      return create_variables<1>([&](auto&& build, std::uint64_t index) { build(std::move(info)); return index; });
    }
  };
}

#endif
