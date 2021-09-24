#include "solve_routine.hpp"
#include "../Utility/indirect.hpp"

namespace expression::solver {
  namespace {
    template<class T>
    auto enumerated(T&& range) {
      using It = decltype(range.begin());
      using Val = decltype(*range.begin());
      struct WrappedIt {
        It base;
        std::uint64_t count;
        bool operator!=(WrappedIt const& other) const { return base != other.base; }
        void operator++() {
          ++base;
          ++count;
        }
        std::pair<std::uint64_t, Val> operator*() const {
          return {
            count,
            *base
          };
        }
      };
      struct NewRange {
        T range;
        WrappedIt begin() { return WrappedIt{range.begin(), 0}; }
        WrappedIt end() { return WrappedIt{range.end(), (std::uint64_t)-1}; }
      };
      return NewRange{std::forward<T>(range)};
    }
    struct CastInfo {
      std::uint64_t solver_index;
      std::uint64_t type_check_equation;
      compiler::evaluate::Cast* cast;
    };
    struct RuleInfo {
      std::uint64_t solver_index;
      std::uint64_t type_check_equation;
      compiler::evaluate::Rule* rule;
    };
    struct RuleInstance {
      std::uint64_t solver_index;
      compiler::evaluate::Rule* rule;
      compiler::pattern::ConstrainedPattern pattern;
      compiler::evaluate::PatternEvaluateResult evaluated;
      std::optional<std::uint64_t> check_solver;
      bool done = false;
    };
    std::optional<expression::Rule> convert_to_rule(expression::tree::Expression const& pattern, expression::tree::Expression const& replacement, expression::Context const& context, std::unordered_set<std::uint64_t> const& indeterminates) {
      struct Detail {
        expression::Context const& context;
        std::unordered_set<std::uint64_t> const& indeterminates;
        std::unordered_map<std::uint64_t, std::uint64_t> arg_convert;
        std::optional<expression::pattern::Pattern> convert_to_pattern(expression::tree::Expression const& expr, bool spine) {
          return expr.visit(mdb::overloaded{
            [&](expression::tree::Apply const& apply) -> std::optional<expression::pattern::Pattern> {
              if(auto lhs = convert_to_pattern(apply.lhs, spine)) {
                if(auto rhs = convert_to_pattern(apply.rhs, false)) {
                  return expression::pattern::Apply{
                    .lhs = std::move(*lhs),
                    .rhs = std::move(*rhs)
                  };
                }
              }
              return std::nullopt;
            },
            [&](expression::tree::External const& external) -> std::optional<expression::pattern::Pattern> {
              if(context.external_info[external.external_index].is_axiom != spine && !indeterminates.contains(external.external_index)) {
                return expression::pattern::Fixed{external.external_index};
              } else {
                return std::nullopt;
              }
            },
            [&](expression::tree::Arg const& arg) -> std::optional<expression::pattern::Pattern> {
              if(arg_convert.contains(arg.arg_index)) return std::nullopt;
              auto index = arg_convert.size();
              arg_convert.insert(std::make_pair(arg.arg_index, index));
              return expression::pattern::Wildcard{};
            },
            [&](expression::tree::Data const& data) -> std::optional<expression::pattern::Pattern> {
              return std::nullopt;
            }
          });
        }
        std::optional<expression::tree::Expression> remap_replacement(expression::tree::Expression const& expr) {
          return expression::remap_args(arg_convert, expr);
        }
      };
      Detail detail{context, indeterminates};
      if(auto pat = detail.convert_to_pattern(pattern, true)) {
        if(auto rep = detail.remap_replacement(replacement)) {
          return expression::Rule{std::move(*pat), std::move(*rep)};
        }
      }
      return std::nullopt;
    }
    struct SolverInfo {
      std::uint64_t rule_index;
      bool final_stage = false;
    };
  }
  struct Routine::Impl {
    compiler::evaluate::EvaluateResult& input;
    expression::Context& expression_context;
    StandardSolverContext solver_context;
    std::vector<Solver> solvers;
    std::vector<SolverInfo> solver_info;
    std::vector<CastInfo> casts;
    std::vector<RuleInfo> rules;
    std::vector<mdb::Indirect<RuleInstance> > rule_routines;
    Impl(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext in_solver_context)
      :input(input),
       expression_context(expression_context),
       solver_context(std::move(in_solver_context))
     {
      solvers.emplace_back(mdb::ref(solver_context));
      solver_info.push_back({.rule_index = (std::uint64_t)-1}); //not associated to a rule
      for(auto [var, reason] : input.variables) {
        if(is_variable(reason))
          solvers[0].register_variable(var);
        if(is_indeterminate(reason))
          solver_context.indeterminates.insert(var);
      }
      for(auto& cast : input.casts) {
        auto index = solvers[0].add_equation(cast.stack, cast.source_type, cast.target_type);
        casts.push_back({
          .solver_index = 0,
          .type_check_equation = index,
          .cast = &cast
        });
      }
      for(auto& rule : input.rules) {
        auto index = solvers[0].add_equation(rule.stack, rule.pattern_type, rule.replacement_type);
        rules.push_back({
          .solver_index = 0,
          .type_check_equation = index,
          .rule = &rule
        });
      }
    }
    bool examine_solvers() {
      bool made_progress = false;
      for(auto& solver : solvers) {
        made_progress |= solver.try_to_make_progress();
      }
      return made_progress;
    }
    bool examine_casts() {
      auto cast_erase = std::remove_if(casts.begin(), casts.end(), [&](auto cast_info) {
        if(solvers[cast_info.solver_index].is_equation_satisfied(cast_info.type_check_equation)) {
          auto const& cast = *cast_info.cast;
          solver_context.define_variable(cast.variable, cast.stack.depth(), cast.source);
          return true;
        } else {
          return false;
        }
      });
      if(cast_erase != casts.end()) {
        casts.erase(cast_erase, casts.end());
        return true;
      } else {
        return false;
      }
    }
    bool examine_rules() {
      auto rule_erase = std::remove_if(rules.begin(), rules.end(), [&](auto rule_info) {
        if(solvers[rule_info.solver_index].is_equation_satisfied(rule_info.type_check_equation)) {
          auto& rule = *rule_info.rule;
          rule.pattern = expression_context.reduce(std::move(rule.pattern));
          rule.replacement = expression_context.reduce(std::move(rule.replacement));
          if(auto new_rule = convert_to_rule(rule.pattern, rule.replacement, expression_context, solver_context.indeterminates)) {
            expression_context.add_rule(std::move(*new_rule));
            return true;
          } else if(auto constrained_pat = compiler::pattern::from_expression(rule.pattern, expression_context, solver_context.indeterminates)) {
            auto archived = archive(constrained_pat->pattern);
            auto evaluated = compiler::evaluate::evaluate_pattern(archived.root(), expression_context);
            auto solve_index = solvers.size();
            solvers.emplace_back(mdb::ref(solver_context));
            solver_info.push_back({.rule_index = (std::uint64_t)(&rule - input.rules.data())});
            auto& solver = solvers.back();
            rule_routines.emplace_back(mdb::in_place, RuleInstance{
              .solver_index = solve_index,
              .rule = &rule,
              .pattern = std::move(*constrained_pat),
              .evaluated = std::move(evaluated)
            });
            auto& rule = *rule_routines.back();
            for(auto& cast : rule.evaluated.casts) {
              auto index = solver.add_equation(cast.stack, cast.source_type, cast.target_type);
              casts.push_back({
                .solver_index = solve_index,
                .type_check_equation = index,
                .cast = &cast
              });
            }
            for(auto const& [var, reason] : rule.evaluated.variables) {
              solver_context.indeterminates.insert(var);
              if(!std::holds_alternative<compiler::evaluate::pattern_variable_explanation::ApplyCast>(reason)) {
                solver.register_variable(var);
              }
            }
            return true;
          }
        }
        return false;
      });
      if(rule_erase != rules.end()) {
        rules.erase(rule_erase, rules.end());
        return true;
      } else {
        return false;
      }
    }
    bool examine_rule_instances() {
      bool made_progress = false;
      for(auto& instance : rule_routines) {
        if(instance->done) continue;
        if(!instance->check_solver) {
          if(solvers[instance->solver_index].is_fully_satisfied()) {
            auto solve_index = solvers.size();
            solvers.emplace_back(mdb::ref(solver_context));
            solver_info.push_back({.rule_index = solver_info[instance->solver_index].rule_index, .final_stage = true});
            auto& solver = solvers.back();
            std::vector<expression::tree::Expression> capture_vec;
            for(auto cast_var : instance->evaluated.capture_point_variables) capture_vec.push_back(expression::tree::External{cast_var});
            for(auto constraint : instance->pattern.constraints) {
              auto base_value = capture_vec[constraint.capture_point];
              auto new_value = expression::substitute_into_replacement(capture_vec, constraint.equivalent_expression);
              solver.add_equation(Stack::empty(expression_context), std::move(base_value), std::move(new_value));
            }
            instance->check_solver = solve_index;
            made_progress = true;
          }
        } else {
          if(solvers[*instance->check_solver].is_fully_satisfied()) {
            struct PatternBuilder {
              static pattern::Pattern from_outline(compiler::pattern::Pattern const& pat) {
                return pat.visit(mdb::overloaded{
                  [&](compiler::pattern::CapturePoint const&) -> pattern::Pattern {
                    return pattern::Wildcard{};
                  },
                  [&](compiler::pattern::Segment const& segment) {
                    pattern::Pattern ret = pattern::Fixed{segment.head};
                    for(auto const& arg : segment.args) {
                      ret = pattern::Apply{std::move(ret), from_outline(arg)};
                    }
                    return ret;
                  }
                });
              }
            };
            auto pat = PatternBuilder::from_outline(instance->pattern.pattern);
            auto replaced_rule = expression::remap_args(
              instance->pattern.args_to_captures,
              instance->rule->replacement
            );
            if(!replaced_rule) std::terminate(); //the heck?
            expression_context.add_rule({
              .pattern = std::move(pat),
              .replacement = std::move(*replaced_rule)
            });
            instance->done = true;
            made_progress = true;
          }
        }
      }
      return made_progress;
    }
    bool step() {
      return examine_solvers() | examine_casts() | examine_rules() | examine_rule_instances();
    }
    void run() {
      while(step());
    }
    std::vector<HungRoutineEquation> get_hung_equations() {
      std::vector<HungRoutineEquation> ret;
      std::uint64_t solve_index = 0;
      for(auto& solver : solvers) {
        auto eqs = solver.get_hung_equations();
        if(solve_index == 0) {
          std::transform(eqs.begin(), eqs.end(), std::back_inserter(ret), [&](HungEquation& eq) {
            if(eq.source_index < input.casts.size()) {
              return HungRoutineEquation{
                .lhs = std::move(eq.lhs),
                .rhs = std::move(eq.rhs),
                .depth = eq.depth,
                .source_kind = SourceKind::cast_equation,
                .source_index = eq.source_index,
                .failed = eq.failed
              };
            } else {
              return HungRoutineEquation{
                .lhs = std::move(eq.lhs),
                .rhs = std::move(eq.rhs),
                .depth = eq.depth,
                .source_kind = SourceKind::rule_equation,
                .source_index = eq.source_index - input.casts.size(),
                .failed = eq.failed
              };
            }
          });
        } else {
          auto const& info = solver_info[solve_index];
          std::transform(eqs.begin(), eqs.end(), std::back_inserter(ret), [&](HungEquation& eq) {
            return HungRoutineEquation{
              .lhs = std::move(eq.lhs),
              .rhs = std::move(eq.rhs),
              .depth = eq.depth,
              .source_kind = info.final_stage ? SourceKind::rule_skeleton_verify : SourceKind::rule_skeleton,
              .source_index = info.rule_index,
              .failed = eq.failed
            };
          });
          //???
        }
        ++solve_index;
      }
      return ret;
    }
  };
  Routine::Routine(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext solver_context):impl(std::make_unique<Impl>(input, expression_context, std::move(solver_context))) {}
  Routine::Routine(Routine&&) = default;
  Routine& Routine::operator=(Routine&&) = default;
  Routine::~Routine() = default;
  void Routine::run() { return impl->run(); }
  std::vector<HungRoutineEquation> Routine::get_hung_equations() { return impl->get_hung_equations(); }
}
