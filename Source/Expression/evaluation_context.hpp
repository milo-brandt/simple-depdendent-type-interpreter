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
  constexpr auto multi_apply = [](tree::Expression head, auto&&... args) {
    ((head = tree::Apply{std::move(head), std::move(args)}) , ...);
    return head;
  };
  struct Rule {
    pattern::Pattern pattern;
    tree::Expression replacement;
  };
  struct DataRule {
    data_pattern::Pattern pattern;
    mdb::function<tree::Expression(std::vector<tree::Expression>)> replace;
  };
  struct ExternalInfo {
    bool is_axiom;
    tree::Expression type;
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
  };
  struct Context {
    std::vector<Rule> rules;
    std::vector<DataRule> data_rules;
    std::vector<ExternalInfo> external_info;
    Primitives primitives;
    Context();
    TypedValue get_external(std::uint64_t);
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
