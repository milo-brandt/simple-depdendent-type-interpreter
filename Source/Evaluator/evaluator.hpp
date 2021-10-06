#ifndef EVALUATOR_INSTANCE_HPP
#define EVALUATOR_INSTANCE_HPP

#include "../Expression/expression_tree.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace evaluator {
  struct Replacement {
    expression::tree::Expression pattern;
    expression::tree::Expression replacement;
    std::uint64_t associated_mark = -1;
  };
  struct TreeHasher {
    std::hash<void const*> h1;
    auto operator()(expression::tree::Expression const& expr) const noexcept {
      return h1(expr.data());
    }
  };

  struct Instance {
    std::vector<bool> is_external_axiom;
    std::vector<expression::Rule> rules;
    std::vector<expression::DataRule> data_rules;
    std::vector<Replacement> replacements;
    std::unordered_map<expression::tree::Expression, std::unordered_set<std::uint64_t>, TreeHasher> marks;

    expression::tree::Expression reduce(expression::tree::Expression, std::size_t ignore_replacement = (std::size_t)-1);
    bool is_purely_open(expression::tree::Expression reduced_expr);
    bool is_axiomatic(expression::tree::Expression reduced_expr);

    static Instance create(
      std::vector<bool> is_external_axiom,
      std::vector<expression::Rule> rules,
      std::vector<expression::DataRule> data_rules,
      std::vector<Replacement> replacements
    );
  };
}

#endif
