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
      mdb::function<void(compiler::pattern::ConstrainedPattern)> then;
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
    template<class T, class Then>
    void request(T request, Then then) {
      solver.request(std::move(request), std::move(then));
    }
    template<class Then>
    void request(request::ConstrainPattern pattern, Then then) {
      waiting_constrained_patterns.push_back(PatternConstrainer{
        std::move(pattern.pattern),
        std::move(then)
      });
    }
    auto rule_routine(compiler::evaluate::Rule rule) {
      struct RuleInfo {
        compiler::pattern::ConstrainedPattern pattern;
        compiler::evaluate::PatternEvaluateResult evaluated;
      };
      return mdb::async::bind<mdb::Unit>(
        request::Solve{.equation = Equation{rule.stack, rule.pattern_type, rule.replacement_type}},
        [rule, this](auto&& ret, mdb::Unit) {
          auto pat = expression_context.reduce(rule.pattern);
          auto rep = expression_context.reduce(rule.replacement);
          if(auto new_rule = convert_to_rule(pat, rep, expression_context, solver_context.indeterminates)) {
            expression_context.add_rule(std::move(*new_rule));
            return ret(mdb::async::pure(mdb::unit));
          } else {
            return ret(mdb::async::bind<mdb::Unit>(
              request::ConstrainPattern{.pattern = rule.pattern},
              [this, rule](auto&& ret, compiler::pattern::ConstrainedPattern constrained_pat) {
                auto archived = archive(constrained_pat.pattern);
                auto evaluated = compiler::evaluate::evaluate_pattern(archived.root(), expression_context);
                std::unordered_set<std::uint64_t> solver_indeterminates;
                for(auto const& [var, reason] : evaluated.variables) {
                  solver_context.indeterminates.insert(var);
                  if(!std::holds_alternative<compiler::evaluate::pattern_variable_explanation::ApplyCast>(reason)) {
                    solver_indeterminates.insert(var);
                  }
                }
                return ret(mdb::async::bind<mdb::Unit>(
                  request::CreateContext{.indeterminates = std::move(solver_indeterminates)},
                  [this, rule_info = RuleInfo{.pattern = std::move(constrained_pat), .evaluated = std::move(evaluated)}](auto&& ret, IndeterminateContext context) {
                    ret(mdb::async::complex<RuleInfo>([&]<class Then>(auto&& spawn, Then then) {
                      struct SharedState {
                        Then then;
                        std::uint64_t count_left;
                        RuleInfo info;
                        void finish() { std::move(then)(std::move(info)); }
                        SharedState(Then then, std::uint64_t count_left, RuleInfo info):then(std::move(then)),count_left(count_left),info(std::move(info)) {
                          if(count_left == 0) finish();
                        }
                        auto decrement() {
                          if(--count_left == 0)
                            finish();
                        }
                      };
                      auto size = rule_info.evaluated.casts.size();
                      auto shared = std::make_shared<SharedState>(std::move(then), size, std::move(rule_info));
                      auto& rule = shared->info;
                      auto decrementor = [shared](auto&&...) { shared->decrement(); };
                      for(auto const& cast : rule.evaluated.casts) {
                        spawn(mdb::async::bind<mdb::Unit>(
                          request::Solve{.indeterminate_context = context, .equation = Equation{cast.stack, cast.source_type, cast.target_type}},
                          [cast, this](auto&& ret, mdb::Unit) {
                            solver_context.define_variable(cast.variable, cast.stack.depth(), cast.source);
                            ret(mdb::async::pure(mdb::unit));
                          }
                        ), decrementor);
                      }
                    }));
                  },
                  [this](auto&& ret, RuleInfo rule_info) {
                    ret(mdb::async::map(
                      request::CreateContext{}, //make a context with no indeterminates
                      [rule_info = std::move(rule_info)](auto context) mutable { return std::make_pair(context, std::move(rule_info)); }
                    ));
                  },
                  [this](auto&& ret, std::pair<IndeterminateContext, RuleInfo> info) {
                    auto& context = info.first;
                    auto& rule_info = info.second;
                    ret(mdb::async::complex<RuleInfo>([&]<class Then>(auto&& spawn, Then then) {
                      struct SharedState {
                        Then then;
                        std::uint64_t count_left;
                        RuleInfo info;
                        void finish() { std::move(then)(std::move(info)); }
                        SharedState(Then then, std::uint64_t count_left, RuleInfo info):then(std::move(then)),count_left(count_left),info(std::move(info)) {
                          if(count_left == 0) finish();
                        }
                        auto decrement() { if(--count_left == 0) finish(); }
                      };
                      auto size = rule_info.pattern.constraints.size();
                      auto shared = std::make_shared<SharedState>(std::move(then), size, std::move(rule_info));
                      auto& rule = shared->info;
                      std::vector<expression::tree::Expression> capture_vec;
                      auto decrementor = [shared](auto&&...) { shared->decrement(); };
                      for(auto cast_var : rule.evaluated.capture_point_variables) capture_vec.push_back(expression::tree::External{cast_var});
                      for(auto constraint : rule.pattern.constraints) {
                        auto base_value = capture_vec[constraint.capture_point];
                        auto new_value = expression::substitute_into_replacement(capture_vec, constraint.equivalent_expression);
                        spawn(
                          request::Solve{.indeterminate_context = context, .equation = Equation{Stack::empty(expression_context), std::move(base_value), std::move(new_value)}},
                          decrementor
                        );
                      }
                    }));
                  },
                  [this, rule](auto&& ret, RuleInfo rule_info) {
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
                    auto pat = PatternBuilder::from_outline(rule_info.pattern.pattern);
                    auto replaced_rule = expression::remap_args(
                      rule_info.pattern.args_to_captures,
                      rule.replacement
                    );
                    if(!replaced_rule) std::terminate(); //the heck?
                    expression_context.add_rule({
                      .pattern = std::move(pat),
                      .replacement = std::move(*replaced_rule)
                    });
                    ret(mdb::async::pure(mdb::unit));
                  }
                ));
              }
            ));
          }
        }
      );
    }
    Impl(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext in_solver_context)
      :input(input),
       expression_context(expression_context),
       solver_context(std::move(in_solver_context)),
       solver(mdb::ref(solver_context))
     {
      for(auto [var, reason] : input.variables) {
        if(is_variable(reason))
          mdb::async::execute(mdb::ref(*this), request::RegisterIndeterminate{.new_variable = var});
        if(is_indeterminate(reason))
          solver_context.indeterminates.insert(var);
      }
      for(auto& cast : input.casts) {
        mdb::async::execute(mdb::ref(*this), mdb::async::bind<mdb::Unit>(
          request::Solve{.equation = Equation{cast.stack, cast.source_type, cast.target_type}},
          [cast, this](auto&& ret, mdb::Unit) {
            solver_context.define_variable(cast.variable, cast.stack.depth(), cast.source);
            ret(mdb::async::pure(mdb::unit));
          }
        ));
      }
      for(auto& rule : input.rules) {
        mdb::async::execute(mdb::ref(*this), rule_routine(rule));
      }
    }
    bool examine_solvers() {
      return solver.try_to_make_progress();
    }
    bool examine_constrained_patterns() {
      return mdb::erase_from_active_queue(waiting_constrained_patterns, [&](auto& waiting) {
        waiting.pattern = expression_context.reduce(std::move(waiting.pattern));
        if(auto pat = compiler::pattern::from_expression(waiting.pattern, expression_context, solver_context.indeterminates)) {
          waiting.then(std::move(*pat));
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
    }
  };
  Routine::Routine(compiler::evaluate::EvaluateResult& input, expression::Context& expression_context, StandardSolverContext solver_context):impl(std::make_unique<Impl>(input, expression_context, std::move(solver_context))) {}
  Routine::Routine(Routine&&) = default;
  Routine& Routine::operator=(Routine&&) = default;
  Routine::~Routine() = default;
  void Routine::run() { return impl->run(); }
}
