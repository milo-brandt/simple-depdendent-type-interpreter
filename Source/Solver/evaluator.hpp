#ifndef COMPILER_EVALUATOR_HPP
#define COMPILER_EVALUATOR_HPP

#include "../Compiler/new_instructions.hpp"
#include "stack.hpp"
#include "../NewExpression/evaluation.hpp"
#include "../NewExpression/typed_value.hpp"
#include "../NewExpression/type_utility.hpp"
#include "interface_types.hpp"

namespace solver::evaluator {
  struct EvaluatorInterface {
    new_expression::Arena& arena;
    new_expression::RuleCollector& rule_collector;
    new_expression::PrimitiveTypeCollector& type_collector;
    new_expression::WeakExpression type;
    new_expression::WeakExpression arrow;
    new_expression::WeakExpression arrow_type;
    new_expression::WeakExpression type_family;
    new_expression::WeakExpression id;
    new_expression::WeakExpression constant;
    std::function<void(new_expression::WeakExpression, new_expression::OwnedExpression)> register_type;
    std::function<void(new_expression::WeakExpression)> register_declaration;
    std::function<void(new_expression::OwnedExpression)> register_definable_indeterminate;
    std::function<void(new_expression::Rule)> add_rule;
    mdb::function<new_expression::OwnedExpression(new_expression::OwnedExpression)> reduce;
    mdb::function<void(Cast)> cast;
    mdb::function<void(FunctionCast)> function_cast;
    mdb::function<void(Rule)> rule;
    mdb::function<new_expression::TypedValue(std::uint64_t)> embed;
  };
  new_expression::TypedValue evaluate(compiler::new_instruction::output::archive_part::ProgramRoot const& tree, EvaluatorInterface interface);
}

#endif
