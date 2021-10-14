#ifndef SOLVER_MANAGER_HPP
#define SOLVER_MANAGER_HPP

#include "../Utility/function.hpp"
#include "../Utility/async.hpp"
#include "../NewExpression/type_theory_primitives.hpp"
#include "evaluator.hpp"
#include "solver.hpp"

namespace solver {
  enum class EquationResult {
    failed,
    stalled,
    solved
  };
  struct BasicContext {
    new_expression::Arena& arena;
    new_expression::RuleCollector rule_collector;
    new_expression::TypeTheoryPrimitives primitives;
    BasicContext(new_expression::Arena&);
    BasicContext(BasicContext&&) = default;
    BasicContext& operator=(BasicContext&&) = default;
    ~BasicContext();
  };
  class Manager {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Manager(BasicContext&);
    Manager(Manager&&);
    Manager& operator=(Manager&&);
    ~Manager();
    void register_definable_indeterminate(new_expression::OwnedExpression);
    new_expression::OwnedExpression reduce(new_expression::OwnedExpression);
    mdb::Future<EquationResult> register_equation(Equation);
    mdb::Future<EquationResult> register_cast(Cast);
    mdb::Future<EquationResult> register_function_cast(FunctionCast);
    mdb::Future<bool> register_rule(Rule);
    void run();
    void close();
    evaluator::EvaluatorInterface get_evaluator_interface(mdb::function<new_expression::TypedValue(std::uint64_t)> embed);
  };
}

#endif
