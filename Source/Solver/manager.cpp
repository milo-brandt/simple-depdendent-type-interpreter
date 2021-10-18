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
    primitives(arena, rule_collector),
    type_collector(arena)
  {
    for(auto& pair : primitives.get_types_of_primitives(arena)) {
      type_collector.type_of_primitive.set(pair.first, std::move(pair.second));
    }
  }
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
    };
    struct RuleBuilder {
      new_expression::OwnedExpression pattern;
      new_expression::OwnedExpression replacement;
      std::optional<mdb::Promise<bool> > result;
    };
  }
  struct Manager::Impl {
    Arena& arena;
    RuleCollector& rule_collector;
    new_expression::EvaluationContext evaluation;
    new_expression::TypeTheoryPrimitives& primitives;
    new_expression::PrimitiveTypeCollector& type_collector;
    std::vector<EquationSolver> active_solvers;
    std::vector<RuleBuilder> active_rule_builders;
    new_expression::OwnedKeySet definable_indeterminates;
    SegmentResult run(EquationSolver& eq) {
      auto made_progress = eq.solver.try_to_make_progress();
      if(eq.result) {
        if(eq.solver.solved()) {
          eq.result->set_value(EquationResult::solved);
          eq.result = std::nullopt;
          return {
            .made_progress = true,
            .done = true
          };
        } else if(eq.solver.failed()) {
          eq.result->set_value(EquationResult::failed);
          eq.result = std::nullopt;
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
    SegmentResult run(RuleBuilder& rule) {
      //just handle lambdas for now.
      rule.pattern = evaluation.reduce(std::move(rule.pattern));
      auto unfolded = unfold(arena, rule.pattern);
      if(arena.holds_declaration(unfolded.head)) {
        for(std::size_t i = 0; i < unfolded.args.size(); ++i) {
          if(auto* arg = arena.get_if_argument(unfolded.args[i])) {
            if(arg->index != i) {
              return {
                .made_progress = false,
                .done = false
              };
            }
          }
        }
        rule_collector.add_rule({
          .pattern = new_expression::lambda_pattern(arena.copy(unfolded.head), unfolded.args.size()),
          .replacement = std::move(rule.replacement)
        });
        destroy_from_arena(arena, rule.pattern);
        rule.result->set_value(true);
        return {
          .made_progress = true,
          .done = true
        };
      }
      return {
        .made_progress = false,
        .done = false
      };
    }
    SolverInterface get_solver_interface() {
      return {
        .arena = arena,
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
        .stack = cast.stack
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
        .stack = cast.stack
      }).listen([
        this,
        function_variable = std::move(cast.function_variable),
        argument_variable = std::move(cast.argument_variable),
        function_value = std::move(cast.function_value),
        argument_value = std::move(cast.argument_value),
        argument_type = std::move(cast.argument_type),
        expected_argument_type = std::move(cast.expected_argument_type),
        stack = std::move(cast.stack),
        promise = std::move(promise)
      ](EquationResult result) mutable {
        if(result == EquationResult::solved) {
          rule_collector.add_rule({
            .pattern = new_expression::lambda_pattern(std::move(function_variable), stack.depth()),
            .replacement = std::move(function_value)
          });
          register_equation({
            .lhs = std::move(argument_type),
            .rhs = std::move(expected_argument_type),
            .stack = stack
          }).listen([
            this,
            argument_variable = std::move(argument_variable),
            argument_value = std::move(argument_value),
            stack = std::move(stack),
            promise = std::move(promise)
          ](EquationResult result) mutable {
            if(result == EquationResult::solved) {
              rule_collector.add_rule({
                .pattern = new_expression::lambda_pattern(std::move(argument_variable), stack.depth()),
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
      struct SharedState {
        Impl& me;
        new_expression::Rule rule;
        mdb::Promise<bool> promise;
        std::size_t equations_left;
        SharedState(Impl& me, new_expression::Rule rule, mdb::Promise<bool> promise, std::size_t equations_left):me(me), rule(std::move(rule)), promise(std::move(promise)), equations_left(equations_left) {}
        void mark_solve() {
          if(--equations_left == 0) {
            me.rule_collector.add_rule(std::move(rule));
            promise.set_value(true);
          }
        }
        ~SharedState() {
          if(equations_left > 0) {
            destroy_from_arena(me.arena, rule);
            promise.set_value(false);
          }
        }
      };
      auto [promise, future] = mdb::create_promise_future_pair<bool>();
      if(rule.checks.empty()) {
        rule_collector.add_rule(std::move(rule.rule));
        promise.set_value(true);
      } else {
        auto shared = std::make_shared<SharedState>(*this, std::move(rule.rule), std::move(promise), rule.checks.size());
        for(auto& check : rule.checks) {
          register_equation({
            .lhs = std::move(check.first),
            .rhs = std::move(check.second),
            .stack = rule.stack
          }).listen([shared](EquationResult result) {
            if(result == EquationResult::solved) {
              shared->mark_solve();
            }
          });
        }
      }
      return std::move(future);
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
          return extract_done(run(eq));
        });
        mdb::erase_from_active_queue(active_rule_builders, [&](auto& rule) {
          return extract_done(run(rule));
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
      mdb::erase_from_active_queue(active_rule_builders, [&](auto& eq) {
        if(eq.result) {
          eq.result->set_value(false);
        }
        return true;
      });
    }
    evaluator::EvaluatorInterface get_evaluator_interface(ExternalInterfaceParts external_interface) {
      return {
        .arena = arena,
        .rule_collector = rule_collector,
        .type_collector = type_collector,
        .type = primitives.type,
        .arrow = primitives.arrow,
        .arrow_type = primitives.arrow_type,
        .type_family = primitives.type_family,
        .id = primitives.id,
        .constant = primitives.constant,
        .register_type = [this](WeakExpression primitive, OwnedExpression type) {
          type_collector.type_of_primitive.set(primitive, std::move(type));
        },
        .register_declaration = [this](WeakExpression expr) {
          rule_collector.register_declaration(expr);
        },
        .register_definable_indeterminate = [this](OwnedExpression expr) {
          definable_indeterminates.insert(std::move(expr));
        },
        .add_rule = [this](new_expression::Rule rule) {
          rule_collector.add_rule(std::move(rule));
        },
        .explain_variable = std::move(external_interface.explain_variable),
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
        .embed = std::move(external_interface.embed)
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
    .type_collector{context.type_collector},
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
  evaluator::EvaluatorInterface Manager::get_evaluator_interface(ExternalInterfaceParts external_interface) { return impl->get_evaluator_interface(std::move(external_interface)); }
  OwnedExpression Manager::reduce(OwnedExpression expr) {
    return impl->evaluation.reduce(std::move(expr));
  }
  bool Manager::solved() { return impl->solved(); }

}
