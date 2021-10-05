#ifndef EVALUATOR_INSTANCE_HPP
#define EVALUATOR_INSTANCE_HPP

#include "../Expression/expression_tree.hpp"
#include <vector>

namespace evaluator {
  struct Instance {
    std::vector<bool> is_external_axiom;
    std::vector<expression::Rule> rules;
    std::vector<expression::DataRule> data_rules;
    std::vector<std::pair<expression::tree::Expression, expression::tree::Expression> > replacements;

    expression::tree::Expression reduce(expression::tree::Expression, std::size_t ignore_replacement = (std::size_t)-1);
    bool is_purely_open(expression::tree::Expression reduced_expr);
    bool is_axiomatic(expression::tree::Expression reduced_expr);

    static Instance create(
      std::vector<bool> is_external_axiom,
      std::vector<expression::Rule> rules,
      std::vector<expression::DataRule> data_rules,
      std::vector<std::pair<expression::tree::Expression, expression::tree::Expression> > replacements
    );
  };
}

#endif
