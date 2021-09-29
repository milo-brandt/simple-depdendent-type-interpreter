#include "solve_routine.hpp"
#include "../Utility/indirect.hpp"
#include "../Utility/async.hpp"
#include "../Utility/vector_utility.hpp"

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
    struct PatternConstrainer {
      tree::Expression pattern;
      mdb::Promise<std::optional<compiler::pattern::ConstrainedPattern> > promise;
    };
  }
  namespace request {
    struct ConstrainPattern {
      using RoutineType = compiler::pattern::ConstrainedPattern;
      static constexpr bool is_primitive = true;
      tree::Expression pattern;
    };
  }
  struct Routine::Impl {
    compiler::evaluate::EvaluateResult& input;
    expression::Context& expression_context;
    StandardSolverContext solver_context;
    Solver solver;
    std::vector<PatternConstrainer> waiting_constrained_patterns;
    std::vector<HungRoutineEquation> hung_equations;
    mdb::Future<std::optional<compiler::pattern::ConstrainedPattern> > constrain_pattern(request::ConstrainPattern pattern) {
      auto [promise, future] = mdb::create_promise_future_pair<std::optional<compiler::pattern::ConstrainedPattern> >();
      waiting_constrained_patterns.push_back(PatternConstrainer{
        std::move(pattern.pattern),
        std::move(promise)
      });
      return std::move(future);
    }
    static std::optional<SolveError> join_result_vector(std::vector<std::optional<SolveError> > results) {
      for(auto result : results) {
        if(result) {
          return result;
        }
      }
      return std::nullopt;
    }
    auto rule_routine(std::uint64_t index, compiler::evaluate::Rule rule) {
      struct RuleInfo {
        compiler::pattern::ConstrainedPattern pattern;
        compiler::evaluate::PatternEvaluateResult evaluated;
      };
      solver.solve({
        .equation = Equation{rule.stack, rule.pattern_type, rule.replacement_type}}
      ).listen([this, rule = std::move(rule), index](std::optional<SolveError> error) {
        if(error) {
          hung_equations.push_back({
            .equation_base_index = error->equation_id,
            .source_kind = SourceKind::rule_equation,
            .source_index = index,
            .failed = error->failed
          });
          std::cout << "Bad equation rule!\n";
          return;
        }
        auto pat = expression_context.reduce(rule.pattern);
        auto rep = expression_context.reduce(rule.replacement);
        if(auto new_rule = convert_to_rule(pat, rep, expression_context, solver_context.indeterminates)) {
          expression_context.add_rule(std::move(*new_rule));
        } else {
          constrain_pattern({.pattern = std::move(pat)}).listen([this, rule = std::move(rule)](std::optional<compiler::pattern::ConstrainedPattern> constrained_pat) {
            if(!constrained_pat) {
              std::cout << "Unconstrainable pattern!\n";
              return;
            }
            auto archived = archive(constrained_pat->pattern);
            auto evaluated = compiler::evaluate::evaluate_pattern(archived.root(), expression_context);
            std::unordered_set<std::uint64_t> solver_indeterminates;
            for(auto const& [var, reason] : evaluated.variables) {
              solver_context.indeterminates.insert(var);
              if(!std::holds_alternative<compiler::evaluate::pattern_variable_explanation::ApplyCast>(reason)) {
                solver_indeterminates.insert(var);
              }
            }
            auto context = solver.create_context({
              .indeterminates = std::move(solver_indeterminates)
            });
            std::vector<mdb::Future<std::optional<SolveError> > > results;
            for(auto const& cast : evaluated.casts) {
              results.push_back(solver.solve({
                .indeterminate_context = context,
                .equation = {
                  cast.stack,
                  cast.source_type,
                  cast.target_type
                }
              }).then([this, cast](std::optional<SolveError> error) {
                if(!error) {
                  solver_context.define_variable(cast.variable, cast.stack.depth(), cast.source);
                }
                return error;
              }));
            }
            collect(std::move(results)).then(&join_result_vector).listen([this, rule = std::move(rule), constrained_pat = std::move(*constrained_pat), evaluated = std::move(evaluated)](std::optional<SolveError> error) {
              if(error) {
                std::cout << "Bad equation capture kind!\n";
                for(auto const& cast : evaluated.casts) {
                  auto lhs = expression_context.reduce(cast.source_type);
                  auto rhs = expression_context.reduce(cast.target_type);
                  std::size_t i = 0; //let me look at them, I guess?
                }
                return;
              }
              std::vector<expression::tree::Expression> capture_vec;
              for(auto cast_var : evaluated.capture_point_variables) capture_vec.push_back(expression::tree::External{cast_var});
              std::vector<mdb::Future<std::optional<SolveError> > > results;
              auto context = solver.create_context({});
              for(auto constraint : constrained_pat.constraints) {
                auto base_value = capture_vec[constraint.capture_point];
                auto new_value = expression::substitute_into_replacement(capture_vec, constraint.equivalent_expression);
                results.push_back(solver.solve({
                  .indeterminate_context = context,
                  .equation = Equation{
                    Stack::empty(expression_context),
                    std::move(base_value),
                    std::move(new_value)
                  }
                }));
              }
              collect(std::move(results)).then(&join_result_vector).listen([this, rule = std::move(rule), constrained_pat = std::move(constrained_pat), evaluated = std::move(evaluated)](std::optional<SolveError> error) {
                if(error) {
                  std::cout << "Bad equation constraint check!\n";
                  return;
                }
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
                auto pat = PatternBuilder::from_outline(constrained_pat.pattern);
                auto replaced_rule = expression::remap_args(
                  constrained_pat.args_to_captures,
                  rule.replacement
                );
                if(!replaced_rule) std::terminate(); //the heck?
                expression_context.add_rule({
                  .pattern = std::move(pat),
                  .replacement = std::move(*replaced_rule)
                });
              });
            });
          });
        }
      });
    }
    Impl(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext in_solver_context)
      :input(input),
       expression_context(expression_context),
       solver_context(std::move(in_solver_context)),
       solver(mdb::ref(solver_context))
     {
      for(auto [var, reason] : input.variables) {
        if(is_variable(reason))
          solver.register_indeterminate(request::RegisterIndeterminate{.new_variable = var});
        if(is_indeterminate(reason))
          solver_context.indeterminates.insert(var);
      }
      std::uint64_t cast_index = 0;
      for(auto& cast : input.casts) {
        solver.solve(
          request::Solve{.equation = Equation{cast.stack, cast.source_type, cast.target_type}}
        ).listen([cast, cast_index, this](std::optional<SolveError> error) {
          if(!error) {
            solver_context.define_variable(cast.variable, cast.stack.depth(), cast.source);
          } else {
            hung_equations.push_back({
              .equation_base_index = error->equation_id,
              .source_kind = SourceKind::cast_equation,
              .source_index = cast_index,
              .failed = error->failed
            });
            std::cout << "Bad cast.\n";
          }
        });
        ++cast_index;
      }
      std::uint64_t rule_index = 0;
      for(auto& rule : input.rules) {
        rule_routine(rule_index++, rule);
      }
    }
    bool examine_solvers() {
      return solver.try_to_make_progress();
    }
    bool examine_constrained_patterns() {
      return mdb::erase_from_active_queue(waiting_constrained_patterns, [&](auto& waiting) {
        waiting.pattern = expression_context.reduce(std::move(waiting.pattern));
        if(auto pat = compiler::pattern::from_expression(waiting.pattern, expression_context, solver_context.indeterminates)) {
          waiting.promise.set_value(std::move(*pat));
          return true;
        } else {
          return false;
        }
      });
    }
    bool step() {
      return examine_solvers() | examine_constrained_patterns();
    }
    void run() {
      while(step());
      std::cout << "Done.\n";
      solver.close();
    }
  };
  Routine::Routine(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext solver_context):impl(std::make_unique<Impl>(input, expression_context, std::move(solver_context))) {}
  Routine::Routine(Routine&&) = default;
  Routine& Routine::operator=(Routine&&) = default;
  Routine::~Routine() = default;
  void Routine::run() { return impl->run(); }
  std::vector<HungRoutineEquationInfo> Routine::get_hung_equations() {
    std::vector<HungRoutineEquationInfo> ret;
    for(auto& hung : impl->hung_equations) {
      ret.push_back({
        .info = impl->solver.get_error_info(hung.equation_base_index),
        .source_kind = hung.source_kind,
        .source_index = hung.source_index,
        .failed = hung.failed
      });
    }
    return ret;
  }
}
