#include "manager.hpp"
#include "../Utility/vector_utility.hpp"
#include "../NewExpression/arena_utility.hpp"

namespace solver {
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Arena = new_expression::Arena;
  using RuleCollector = new_expression::RuleCollector;
  using Stack = stack::Stack;
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
      mdb::Promise<EquationResult> result;
    };
    struct EquationFailedSolver {
      Solver solver;
      mdb::Promise<EquationErrorInfo> result;
    };
  }
  struct Manager::Impl {
    Arena& arena;
    RuleCollector& rule_collector;
    new_expression::EvaluationContext evaluation;
    new_expression::TypeTheoryPrimitives& primitives;
    new_expression::PrimitiveTypeCollector& type_collector;
    std::vector<EquationSolver> active_solvers;
    std::vector<EquationFailedSolver> failed_solvers;
    new_expression::OwnedKeyMap<solver::DefinableInfo> definable_indeterminates;
    bool is_running = false;
    SegmentResult run(EquationSolver& eq) {
      auto made_progress = eq.solver.try_to_make_progress();
      if(eq.solver.solved()) {
        eq.result.set_value(EquationSolved{});
        return {
          .made_progress = true,
          .done = true
        };
      } else if(eq.solver.failed()) {
        auto [promise, future] = mdb::create_promise_future_pair<EquationErrorInfo>();
        eq.result.set_value(EquationFailed{
          std::move(future)
        });
        failed_solvers.push_back({
          std::move(eq.solver),
          std::move(promise)
        });
        return {
          .made_progress = true,
          .done = true
        };
      }
      return {
        .made_progress = made_progress,
        .done = false
      };
    }
    SolverInterface get_solver_interface() {
      return {
        .arena = arena,
        .term_depends_on = [this](WeakExpression base, WeakExpression possible_dependency) {
          return base == possible_dependency;
        },
        .get_definable_info = [this](WeakExpression indeterminate) -> std::optional<solver::DefinableInfo> {
          if(definable_indeterminates.contains(indeterminate)) {
            return definable_indeterminates.at(indeterminate);
          } else {
            return std::nullopt;
          }
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
    void run() {
      if(is_running) return; //hack to avoid recursive calls
      is_running = true;
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
        for(auto& failed : failed_solvers) {
          failed.solver.try_to_make_progress();
        }
      }
      is_running = false;
    }
    void close() {
      mdb::erase_from_active_queue(active_solvers, [&](auto& eq) {
        eq.result.set_value(EquationStalled{
          .error = std::move(eq.solver).get_error_info()
        });
        return true;
      });
      mdb::erase_from_active_queue(failed_solvers, [&](auto& eq) {
        eq.result.set_value(std::move(eq.solver).get_error_info());
        return true;
      });
    }
    evaluator::EvaluatorInterface get_evaluator_interface(ExternalInterfaceParts external_interface) {
      return evaluator::EvaluatorInterface{
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
        .register_definable_indeterminate = [this](OwnedExpression expr, Stack stack) {
          definable_indeterminates.set(std::move(expr), solver::DefinableInfo{std::move(stack)});
        },
        .add_rule = [this](new_expression::Rule rule) {
          rule_collector.add_rule(std::move(rule));
        },
        .explain_variable = std::move(external_interface.explain_variable),
        .solve = [this](Equation eq) {
          auto ret = register_equation(std::move(eq));
          run();
          return ret;
        },
        .report_error = std::move(external_interface.report_error),
        .embed = std::move(external_interface.embed),
        .close_interface = [this]() { close(); }
      };
    }
    void register_definable_indeterminate(new_expression::OwnedExpression expr, Stack stack) {
      definable_indeterminates.set(std::move(expr), solver::DefinableInfo{std::move(stack)});
    }
    bool solved() {
      return active_solvers.empty() && failed_solvers.empty();
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

  void Manager::register_definable_indeterminate(OwnedExpression expr, Stack stack) {
    return impl->register_definable_indeterminate(std::move(expr), std::move(stack));
  }
  void Manager::run() { return impl->run(); }
  void Manager::close() { return impl->close(); }
  evaluator::EvaluatorInterface Manager::get_evaluator_interface(ExternalInterfaceParts external_interface) { return impl->get_evaluator_interface(std::move(external_interface)); }
  OwnedExpression Manager::reduce(OwnedExpression expr) {
    return impl->evaluation.reduce(std::move(expr));
  }
  bool Manager::solved() { return impl->solved(); }

}
