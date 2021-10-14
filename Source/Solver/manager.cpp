#include "manager.hpp"
#include "../Utility/vector_utility.hpp"
#include "../NewExpression/arena_utility.hpp"

namespace solver {
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Arena = new_expression::Arena;
  using RuleCollector = new_expression::RuleCollector;

  BasicContext::BasicContext(new_expression::Arena& arena):
    arena(arena),
    rule_collector(arena),
    primitives(arena, rule_collector){}
  BasicContext::~BasicContext() {
    destroy_from_arena(arena, primitives);
  }

  namespace {
    struct SegmentResult {
      bool made_progress;
      bool done;
    };
    struct EquationSolver {
      Solver solver;
      std::optional<mdb::Promise<EquationResult> > result;
      SegmentResult run() {
        auto made_progress = solver.try_to_make_progress();
        if(result) {
          if(solver.solved()) {
            result->set_value(EquationResult::solved);
            result = std::nullopt;
            return {
              .made_progress = true,
              .done = true
            };
          } else if(solver.failed()) {
            result->set_value(EquationResult::failed);
            result = std::nullopt;
            return {
              .made_progress = true,
              .done = false
            };
          }
        }
        return {
          .made_progress = made_progress,
          .done = false
        };
      }
    };
  }
  struct Manager::Impl {
    Arena& arena;
    RuleCollector& rule_collector;
    new_expression::EvaluationContext evaluation;
    new_expression::TypeTheoryPrimitives& primitives;
    std::vector<EquationSolver> active_solvers;
    new_expression::OwnedKeySet definable_indeterminates;
    SolverInterface get_solver_interface() {
      return {
        .arena = arena,
        .reduce = [this](OwnedExpression in) {
          return evaluation.reduce(std::move(in));
        },
        .term_depends_on = [this](WeakExpression base, WeakExpression possible_dependency) {
          return base == possible_dependency;
        },
        .is_definable_indeterminate = [this](WeakExpression indeterminate) {
          return definable_indeterminates.contains(indeterminate);
        },
        .is_lambda_like = [this](WeakExpression expr) {
          return evaluation.is_lambda_like(expr);
        },
        .is_head_closed = [this](WeakExpression expr) {
          auto head = unfold(arena, expr).head;
          return arena.holds_axiom(head) || arena.holds_argument(head);
          //should check that argument can't reduce by assumption
        },
        .make_definition = [this](IndeterminateDefinition definition) {
          if(!definable_indeterminates.contains(definition.head))
            std::terminate();
          definable_indeterminates.erase(definition.head);
          rule_collector.add_rule({
            .pattern = new_expression::lambda_pattern(arena.copy(definition.head), definition.arg_count),
            .replacement = std::move(definition.replacement)
          });
        }
      };
    }
    mdb::Future<EquationResult> register_equation(Equation equation) {
      auto [promise, future] = mdb::create_promise_future_pair<EquationResult>();
      active_solvers.push_back({
        .solver = Solver(get_solver_interface(), std::move(equation)),
        .result = std::move(promise)
      });
      return std::move(future);
    }
    mdb::Future<EquationResult> register_cast(Cast cast) {
      return register_equation({
        .lhs = std::move(cast.source_type),
        .rhs = std::move(cast.target_type),
        .depth = cast.stack.depth()
      }).then([
        this,
        depth = cast.stack.depth(),
        variable = std::move(cast.variable),
        source = std::move(cast.source)
      ](EquationResult result) mutable {
        if(result == EquationResult::solved) {
          rule_collector.add_rule({
            .pattern = new_expression::lambda_pattern(std::move(variable), depth),
            .replacement = std::move(source)
          });
        } else {
          destroy_from_arena(arena, variable, source);
        }
        return result;
      });
    }
    mdb::Future<EquationResult> register_function_cast(FunctionCast cast) {
      auto [promise, future] = mdb::create_promise_future_pair<EquationResult>();
      register_equation({
        .lhs = std::move(cast.function_type),
        .rhs = std::move(cast.expected_function_type),
        .depth = cast.stack.depth()
      }).listen([
        this,
        function_variable = std::move(cast.function_variable),
        argument_variable = std::move(cast.argument_variable),
        function_value = std::move(cast.function_value),
        argument_value = std::move(cast.argument_value),
        argument_type = std::move(cast.argument_type),
        expected_argument_type = std::move(cast.expected_argument_type),
        depth = cast.stack.depth(),
        promise = std::move(promise)
      ](EquationResult result) mutable {
        if(result == EquationResult::solved) {
          rule_collector.add_rule({
            .pattern = new_expression::lambda_pattern(std::move(function_variable), depth),
            .replacement = std::move(function_value)
          });
          register_equation({
            .lhs = std::move(argument_type),
            .rhs = std::move(expected_argument_type),
            .depth = depth
          }).listen([
            this,
            argument_variable = std::move(argument_variable),
            argument_value = std::move(argument_value),
            depth,
            promise = std::move(promise)
          ](EquationResult result) mutable {
            if(result == EquationResult::solved) {
              rule_collector.add_rule({
                .pattern = new_expression::lambda_pattern(std::move(argument_variable), depth),
                .replacement = std::move(argument_value)
              });
            } else {
              destroy_from_arena(arena, argument_variable, argument_value);
            }
            promise.set_value(result);
          });
        } else {
          destroy_from_arena(arena, function_variable, argument_variable, function_value,
            argument_value, argument_type, expected_argument_type);
          promise.set_value(result);
        }
      });
      return std::move(future);
    }
    mdb::Future<bool> register_rule(Rule rule) {
      std::terminate();
      /*return register_equation({
        .lhs = std::move(rule.pattern_type),
        .rhs = std::move(rule.replacement_type),
        .depth = rule.stack.depth()
      }).then([
        this,
        pattern = std::move(rule.pattern),
        replacement = std::move(rule.replacement),
        depth = rule.stack.depth()
      ](EquationResult result) mutable {
        if(result == EquationResult::solved) {

        } else {
          destroy_from_arena(arena, pattern, replacement);
          return false;
        }
      });*/
    }
    void run() {
      bool progress_made = true;
      while(progress_made) {
        progress_made = false;
        auto extract_done = [&](SegmentResult r) {
          progress_made |= r.made_progress;
          return r.done;
        };
        mdb::erase_from_active_queue(active_solvers, [&](auto& eq) {
          return extract_done(eq.run());
        });
      }
    }
    void close() {
      mdb::erase_from_active_queue(active_solvers, [&](auto& eq) {
        if(eq.result) {
          eq.result->set_value(EquationResult::stalled);
        }
        return true;
      });
    }
    evaluator::EvaluatorInterface get_evaluator_interface(mdb::function<new_expression::TypedValue(std::uint64_t)> embed) {
      return {
        .arena = arena,
        .type = primitives.type,
        .arrow = primitives.arrow,
        .arrow_type = primitives.arrow_type,
        .type_family = primitives.type_family,
        .id = primitives.id,
        .register_declaration = [this](WeakExpression expr) {
          rule_collector.register_declaration(expr);
        },
        .register_definable_indeterminate = [this](OwnedExpression expr) {
          definable_indeterminates.insert(std::move(expr));
        },
        .add_rule = [this](new_expression::Rule rule) {
          rule_collector.add_rule(std::move(rule));
        },
        .reduce = [this](OwnedExpression in) {
          return evaluation.reduce(std::move(in));
        },
        .cast = [this](Cast cast) {
          register_cast(std::move(cast));
          run();
        },
        .function_cast = [this](FunctionCast cast) {
          register_function_cast(std::move(cast));
          run();
        },
        .rule = [this](Rule rule) {
          register_rule(std::move(rule));
          run();
        },
        .embed = std::move(embed)
      };
    }
    void register_definable_indeterminate(new_expression::OwnedExpression expr) {
      definable_indeterminates.insert(std::move(expr));
    }
    bool solved() {
      return active_solvers.empty();
    }
  };
  Manager::Manager(BasicContext& context):impl(new Impl{
    .arena = context.arena,
    .rule_collector = context.rule_collector,
    .evaluation{context.arena, context.rule_collector},
    .primitives = context.primitives,
    .definable_indeterminates{context.arena}
  }){}
  Manager::Manager(Manager&&) = default;
  Manager& Manager::operator=(Manager&&) = default;
  Manager::~Manager() = default;

  void Manager::register_definable_indeterminate(OwnedExpression expr) {
    return impl->register_definable_indeterminate(std::move(expr));
  }
  mdb::Future<EquationResult> Manager::register_equation(Equation eq) { return impl->register_equation(std::move(eq)); }
  mdb::Future<EquationResult> Manager::register_cast(Cast cast) { return impl->register_cast(std::move(cast)); }
  mdb::Future<EquationResult> Manager::register_function_cast(FunctionCast cast) { return impl->register_function_cast(std::move(cast)); }
  mdb::Future<bool> Manager::register_rule(Rule rule) { return impl->register_rule(std::move(rule)); }
  void Manager::run() { return impl->run(); }
  void Manager::close() { return impl->close(); }
  evaluator::EvaluatorInterface Manager::get_evaluator_interface(mdb::function<new_expression::TypedValue(std::uint64_t)> embed) { return impl->get_evaluator_interface(std::move(embed)); }
  OwnedExpression Manager::reduce(OwnedExpression expr) {
    return impl->evaluation.reduce(std::move(expr));
  }
  bool Manager::solved() { return impl->solved(); }

}
