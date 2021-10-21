#ifndef SOLVER_MANAGER_HPP
#define SOLVER_MANAGER_HPP

#include "../Utility/function.hpp"
#include "../Utility/async.hpp"
#include "../NewExpression/type_theory_primitives.hpp"
#include "evaluator.hpp"
#include "solver.hpp"

namespace solver {
  struct BasicContext {
    new_expression::Arena& arena;
    new_expression::RuleCollector rule_collector;
    new_expression::TypeTheoryPrimitives primitives;
    new_expression::PrimitiveTypeCollector type_collector;
    BasicContext(new_expression::Arena&);
    BasicContext(BasicContext&&) = default;
    BasicContext& operator=(BasicContext&&) = default;
    ~BasicContext();
  };
  struct ExternalInterfaceParts {
    std::function<void(new_expression::WeakExpression, evaluator::variable_explanation::Any)> explain_variable;
    mdb::function<new_expression::TypedValue(std::uint64_t)> embed;
    mdb::function<void(evaluator::error::Any)> report_error;
  };
  class Manager {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Manager(BasicContext&);
    Manager(Manager&&);
    Manager& operator=(Manager&&);
    ~Manager();
    void register_definable_indeterminate(new_expression::OwnedExpression, stack::Stack);
    new_expression::OwnedExpression reduce(new_expression::OwnedExpression);
    void run();
    void close();
    bool solved(); //returns true if nothing is waiting
    evaluator::EvaluatorInterface get_evaluator_interface(ExternalInterfaceParts);
  };
}

#endif
