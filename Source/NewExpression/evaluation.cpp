#include "evaluation.hpp"
#include "arena_utility.hpp"
#include <algorithm>
#include "../Utility/vector_utility.hpp"
#include "../Utility/overloaded.hpp"

namespace new_expression {
  namespace {
    struct ReduceByPatternContext {
      Arena& arena;
      std::vector<WeakExpression>& weak_stack;
      std::vector<OwnedExpression>& strong_stack;
      std::pair<std::size_t, WeakExpression> unfold(WeakExpression input) {
        auto base_size = weak_stack.size();
        while(auto* apply = arena.get_if_apply(input)) {
          weak_stack.push_back(apply->rhs);
          input = apply->lhs;
        }
        std::reverse(weak_stack.begin() + base_size, weak_stack.end());
        return std::make_pair(weak_stack.size() - base_size, input);
      }
    };
    namespace reduction_step_result {
      struct Finished {};
      struct Reduced {
        OwnedExpression result;
        bool should_reduce_more = true;
      };
      template<class T>
      struct RequestReduce {
        std::size_t strong_stack_index;
        T then;
      };
      template<class T>
      using Any = std::variant<Finished, Reduced, RequestReduce<T> >;
      template<class T, class F, class S = std::invoke_result_t<F, T> >
      Any<S> map(F&& map, Any<T> input) {
        return std::visit(mdb::overloaded{
          [&](Finished&) -> Any<S> { return Finished{}; },
          [&](Reduced& reduced) -> Any<S> { return std::move(reduced); },
          [&](RequestReduce<T>& request) -> Any<S> { return RequestReduce<S>{request.strong_stack_index, map(std::move(request.then))}; }
        }, input);
      }
    };
    struct ReduceByPatternState {
      using Result = reduction_step_result::Any<ReduceByPatternState>;
      std::size_t weak_stack_base;  //layout of stack above this: arg0, arg1, ... , argn, capture0, capture1, ...
      std::size_t weak_stack_next_arg;
      std::size_t weak_stack_pattern_base;
      std::size_t strong_stack_base;
      RuleBody const* current_rule;
      RuleBody const* end_rule;
      PatternStep const* current_step; //should point to a PatternMatch or DataCheck.
      PatternStep const* end_step;
      void cleanup(ReduceByPatternContext context) {
        for(auto it = context.strong_stack.begin() + strong_stack_base; it != context.strong_stack.end(); ++it) {
          context.arena.drop(std::move(*it));
        }
        context.strong_stack.erase(context.strong_stack.begin() + strong_stack_base, context.strong_stack.end());
        context.weak_stack.erase(context.weak_stack.begin() + weak_stack_base, context.weak_stack.end());
      }
      Result pattern_matched(ReduceByPatternContext context) {
        std::span<WeakExpression> pattern_stack{context.weak_stack.data() + weak_stack_pattern_base, context.weak_stack.data() + context.weak_stack.size()};
        auto new_expr = std::visit(mdb::overloaded{
          [&](OwnedExpression const& expr) {
            return substitute_into(context.arena, expr, pattern_stack);
          },
          [&](mdb::function<OwnedExpression(std::span<WeakExpression>)> const& func) {
            return func(pattern_stack);
          }
        }, current_rule->replacement);
        for(;weak_stack_next_arg < weak_stack_pattern_base; ++weak_stack_next_arg) {
          new_expr = context.arena.apply(
            std::move(new_expr),
            context.arena.copy(context.weak_stack[weak_stack_next_arg])
          );
        }
        cleanup(context);
        return reduction_step_result::Reduced{
          std::move(new_expr)
        };
      }
      Result run_step(ReduceByPatternContext context) {
        for(;current_step < end_step; ++current_step) {
          if(auto const* match = std::get_if<PatternMatch>(current_step)) {
            auto new_expr = substitute_into(context.arena, match->substitution, {context.weak_stack.data() + weak_stack_pattern_base, context.weak_stack.data() + context.weak_stack.size()});
            auto strong_stack_index = context.strong_stack.size();
            context.strong_stack.push_back(std::move(new_expr));
            return reduction_step_result::RequestReduce<ReduceByPatternState>{
              .strong_stack_index = strong_stack_index,
              .then = std::move(*this)
            };
          } else if(auto const* check = std::get_if<DataCheck>(current_step)) {
            auto strong_stack_index = context.strong_stack.size();
            context.strong_stack.push_back(context.arena.copy(context.weak_stack[weak_stack_pattern_base + check->capture_index]));
            return reduction_step_result::RequestReduce<ReduceByPatternState>{
              .strong_stack_index = strong_stack_index,
              .then = std::move(*this)
            };
          } else {
            context.weak_stack.push_back(context.weak_stack[weak_stack_next_arg++]);
          }
        }
        return pattern_matched(context); //if we find we need to reduce, we return before reaching here
      }
      Result next_step(ReduceByPatternContext context) {
        ++current_step;
        return run_step(context);
      }
      Result all_patterns_failed(ReduceByPatternContext context) {
        cleanup(context);
        return reduction_step_result::Finished{};
      }
      Result run_pattern(ReduceByPatternContext context) {
      RUN_PATTERN:
        if(current_rule == end_rule) {
          return all_patterns_failed(context);
        } else {
          if(current_rule->pattern_body.args_captured > weak_stack_pattern_base - weak_stack_base) {
            ++current_rule;
            goto RUN_PATTERN;
          }
          current_step = current_rule->pattern_body.steps.data();
          end_step = current_rule->pattern_body.steps.data() + current_rule->pattern_body.steps.size();
          return run_step(context);
        }
      }
      Result pattern_failed(ReduceByPatternContext context) {
        context.weak_stack.erase(context.weak_stack.begin() + weak_stack_pattern_base, context.weak_stack.end());
        ++current_rule;
        weak_stack_next_arg = weak_stack_base;
        return run_pattern(context);
      }
      Result resume(ReduceByPatternContext context, WeakExpression result) {
        //should clean self up if returning Reduced or Finished.
        if(auto const* match = std::get_if<PatternMatch>(current_step)) {
          auto [args, head] = context.unfold(result);
          if(args != match->args_captured || head != match->expected_head) {
            return pattern_failed(context);
          }
          return next_step(context);
        } else {
          auto const& check = std::get<DataCheck>(*current_step);
          if(auto* data = context.arena.get_if_data(result)) {
            if(data->type_index == check.expected_type) {
              return next_step(context);
            }
          }
          return pattern_failed(context);
        }
      }
    };


    template<class Base> //Base must provide Arena& arena and OwnedExpression reduce()
    struct ReducerCRTP {
      Base& me() { return *(Base*)this; }
      bool reduce_by_pattern(OwnedExpression& expr, RuleCollector const& collector) {
        std::vector<WeakExpression> weak_stack;
        std::vector<OwnedExpression> strong_stack;
        ReduceByPatternContext context{
          .arena = me().arena,
          .weak_stack = weak_stack,
          .strong_stack = strong_stack
        };
        auto [args, head] = context.unfold(expr);
        if(me().arena.holds_declaration(head)) {
          auto const& declaration_info = collector.declaration_info(head);
          ReduceByPatternState state{
            .weak_stack_base = 0,
            .weak_stack_next_arg = 0,
            .weak_stack_pattern_base = args,
            .strong_stack_base = 0,
            .current_rule = declaration_info.rules.data(),
            .end_rule = declaration_info.rules.data() + declaration_info.rules.size()
          };
          auto last_result = state.run_pattern(context);
          while(true) {
            if(auto* request_reduce = std::get_if<2>(&last_result)) {
              auto reduced = me().reduce(std::move(context.strong_stack[request_reduce->strong_stack_index]));
              WeakExpression result = reduced;
              context.strong_stack[request_reduce->strong_stack_index] = std::move(reduced);
              last_result = state.resume(context, result);
            } else if(auto* reduced = std::get_if<reduction_step_result::Reduced>(&last_result)) {
              me().arena.drop(std::move(expr));
              expr = std::move(reduced->result);
              return true;
            } else {
              return false;
            }
          }
        } else {
          return false;
        }
      }
    };
    struct ReductionConfiguration {
      using ResumptionKind = ReduceByPatternState;
      RuleCollector const& rule_collector;
      ReduceByPatternState::Result reduce(ReduceByPatternContext context, WeakExpression target) {
        auto base = context.weak_stack.size();
        auto [args, head] = context.unfold(target);
        if(context.arena.holds_declaration(head)) {
          auto const& declaration_info = rule_collector.declaration_info(head);
          return ReduceByPatternState{
            .weak_stack_base = base,
            .weak_stack_next_arg = base,
            .weak_stack_pattern_base = base + args,
            .strong_stack_base = context.strong_stack.size(),
            .current_rule = declaration_info.rules.data(),
            .end_rule = declaration_info.rules.data() + declaration_info.rules.size()
          }.run_pattern(context);
        } else {
          context.weak_stack.erase(context.weak_stack.begin() + base, context.weak_stack.end());
          return reduction_step_result::Finished{};
        }
      }
      ReduceByPatternState::Result resume(ReduceByPatternContext context, ResumptionKind resumer, WeakExpression target) {
        return resumer.resume(context, target);
      }
    };
    struct ArgumentReducer {
      using Result = reduction_step_result::Any<ArgumentReducer>;
      WeakExpression head;
      std::size_t weak_stack_base;
      std::size_t weak_stack_next_arg;
      std::size_t weak_stack_end;
      std::size_t strong_stack_base;
      Result run(ReduceByPatternContext context) {
        if(weak_stack_next_arg != weak_stack_end) {
          //request the next argument
          auto strong_stack_index = context.strong_stack.size();
          context.strong_stack.push_back(context.arena.copy(context.weak_stack[weak_stack_next_arg]));
          return reduction_step_result::RequestReduce<ArgumentReducer> {
            strong_stack_index,
            std::move(*this)
          };
        } else {
          //put the expression back together and cleanup
          auto reduction = context.arena.copy(head);
          for(std::size_t i = 0; i < weak_stack_end - weak_stack_base; ++i) {
            reduction = context.arena.apply(
              std::move(reduction),
              std::move(context.strong_stack[strong_stack_base + i])
            );
          }
          context.strong_stack.erase(context.strong_stack.begin() + strong_stack_base, context.strong_stack.end());
          context.weak_stack.erase(context.weak_stack.begin() + weak_stack_base, context.weak_stack.end());
          return reduction_step_result::Reduced{
            .result = std::move(reduction),
            .should_reduce_more = false
          };
        }
      }
      Result resume(ReduceByPatternContext context, WeakExpression) {
        ++weak_stack_next_arg;
        return run(context);
      }
    };
    struct ArgumentReducerConfig {
      using ResumptionKind = ArgumentReducer;
      ArgumentReducer::Result reduce(ReduceByPatternContext context, WeakExpression target) {
        auto base = context.weak_stack.size();
        auto [args, head] = context.unfold(target);
        return ArgumentReducer{
          .head = head,
          .weak_stack_base = base,
          .weak_stack_next_arg = base,
          .weak_stack_end = context.weak_stack.size(),
          .strong_stack_base = context.strong_stack.size()
        }.run(context);
      }
      ArgumentReducer::Result resume(ReduceByPatternContext context, ResumptionKind resumer, WeakExpression result) {
        return resumer.resume(context, result);
      }
    };
    template<class... Ts>
    struct PipelineReduction {
      std::size_t strong_stack_best;
      std::variant<typename Ts::ResumptionKind...> data;
    };
    template<class... Ts>
    struct PipelineConfig {
      using ResumptionKind = PipelineReduction<Ts...>;
      using Result = reduction_step_result::Any<ResumptionKind>;
      std::tuple<Ts...> implementations;
      PipelineConfig(Ts... ts):implementations(std::move(ts)...) {}
      void cleanup(ReduceByPatternContext context) {
        context.arena.drop(std::move(context.strong_stack.back()));
        context.strong_stack.pop_back();
      }
      template<std::size_t index>
      Result map_result(ReduceByPatternContext context, reduction_step_result::Any<typename std::decay_t<decltype(std::get<index>(implementations))>::ResumptionKind> result, std::size_t strong_stack_best);
      template<std::size_t index>
      Result reduce_impl(ReduceByPatternContext context, std::size_t strong_stack_best) {
        if constexpr(index == sizeof...(Ts)) {
          Result ret = reduction_step_result::Reduced{
            .result = std::move(context.strong_stack.back()),
            .should_reduce_more = false
          };
          context.strong_stack.pop_back();
          return ret;
        } else {
          WeakExpression target = context.strong_stack[strong_stack_best];
          return map_result<index>(context, std::get<index>(implementations).reduce(context, target), strong_stack_best);
        }
      }
      Result reduce(ReduceByPatternContext context, WeakExpression target) {
        auto strong_stack_index = context.strong_stack.size();
        context.strong_stack.push_back(context.arena.copy(target));
        return reduce_impl<0>(context, strong_stack_index);
      }
      template<std::size_t index>
      Result resume_impl(ReduceByPatternContext context, ResumptionKind& resumption, WeakExpression result) {
        if constexpr(index == sizeof...(Ts)) {
          std::terminate(); //unreachable.
        } else {
          if(resumption.data.index() == index) {
            return map_result<index>(context, std::get<index>(implementations).resume(context, std::move(std::get<index>(resumption.data)), result), resumption.strong_stack_best);
          } else {
            return resume_impl<index + 1>(context, resumption, result);
          }
        }
      }
      Result resume(ReduceByPatternContext context, ResumptionKind resumption, WeakExpression result) {
        return resume_impl<0>(context, resumption, result);
      }
    };
    template<class... Ts>
    PipelineConfig(Ts...) -> PipelineConfig<Ts...>;
    template<class... Ts>
    template<std::size_t index>
    typename PipelineConfig<Ts...>::Result PipelineConfig<Ts...>::map_result(ReduceByPatternContext context, reduction_step_result::Any<typename std::decay_t<decltype(std::get<index>(implementations))>::ResumptionKind> result, std::size_t strong_stack_best) {
      if(auto* finished = std::get_if<reduction_step_result::Finished>(&result)) {
        if(context.strong_stack.size() != strong_stack_best + 1) std::terminate();
        return reduce_impl<index + 1>(context, strong_stack_best);
      } else if(auto* reduced = std::get_if<reduction_step_result::Reduced>(&result)) {
        if(context.strong_stack.size() != strong_stack_best + 1) std::terminate();
        if(reduced->should_reduce_more) {
          cleanup(context);
          return std::move(*reduced);
        } else {
          auto& best = context.strong_stack[strong_stack_best];
          context.arena.drop(std::move(best));
          best = std::move(reduced->result);
          return reduce_impl<index + 1>(context, strong_stack_best);
        }
      } else {
        auto& request = std::get<2>(result);
        return reduction_step_result::RequestReduce<ResumptionKind>{
          .strong_stack_index = request.strong_stack_index,
          .then = PipelineReduction<Ts...>{
            .strong_stack_best = strong_stack_best,
            .data = std::move(request.then)
          }
        };
      }
    }
    struct TrivialMemoizer {
      std::optional<OwnedExpression> lookup(WeakExpression key) {
        return std::nullopt;
      }
      void memoize(std::span<OwnedExpression const> preimages, WeakExpression result) {}
    };
    struct WeakKeyMemoizer {
      Arena& arena;
      WeakKeyMap<OwnedExpression, PartDestroyer> results;
      WeakKeyMemoizer(Arena& arena):arena(arena), results(arena) {}
      std::optional<OwnedExpression> lookup(WeakExpression key) {
        if(results.contains(key)) {
          return arena.copy(results.at(key));
        } else {
          return std::nullopt;
        }
      }
      void memoize(std::span<OwnedExpression const> preimages, WeakExpression result) {
        for(auto const& preimage : preimages) {
          results.set(preimage, arena.copy(result));
        }
      }
    };
    struct ReductionInfo {
      std::size_t reduction_index;
      std::size_t preimages_start;
      std::size_t preimages_end;
    };
    template<class ReductionConfiguration, class Memoizer>
    OwnedExpression reduce_generic(Arena& arena, OwnedExpression expr, ReductionConfiguration config, Memoizer memoizer = TrivialMemoizer{}) {
      reduction_step_result::Any<typename ReductionConfiguration::ResumptionKind> last_result;
      std::vector<typename ReductionConfiguration::ResumptionKind> resumptions;
      std::vector<ReductionInfo> reduction_info;
      std::vector<WeakExpression> weak_stack;
      std::vector<OwnedExpression> strong_stack;
      ReduceByPatternContext context{
        .arena = arena,
        .weak_stack = weak_stack,
        .strong_stack = strong_stack
      };
      strong_stack.push_back(std::move(expr));
      auto get_reduction_info_for = [&](std::size_t index) {
        return ReductionInfo{
          index,
          strong_stack.size(),
          strong_stack.size()
        };
      };
      reduction_info.push_back(get_reduction_info_for(0));
      /*
        Algorithm:
          Routine: Reduce the top element of waiting_reduction_indices
            1. Get the result of reduce the top element.
          Routine: Handle a result.
            1. If the result is...
              a. Finished -> Resume the routine that called this one. (Or return if outer)
              b. Reduced -> Replace the relevant strong_stack index. Reduce the top waiting element.
              c. RequestReduce -> Push the resumer on to the stack and push the requested reduction onto the stack. Reduce the top waiting element.
      */

    START_REDUCE_TOP:
      if(auto memoized_result = memoizer.lookup(strong_stack[reduction_info.back().reduction_index])) {
        last_result = reduction_step_result::Reduced{
          .result = std::move(*memoized_result),
          .should_reduce_more = false
        };
      } else {
        last_result = config.reduce(context, strong_stack[reduction_info.back().reduction_index]);
      }
    HANDLE_RESULT:
      if(auto* finished = std::get_if<reduction_step_result::Finished>(&last_result)) {
        WeakExpression result = strong_stack[reduction_info.back().reduction_index];
        /*
          This block of code handles the memoization service.
        */
        if(strong_stack.size() != reduction_info.back().preimages_end) std::terminate(); //preimages should be on top of stack at this point
        memoizer.memoize({
          strong_stack.data() + reduction_info.back().preimages_start,
          strong_stack.data() + reduction_info.back().preimages_end
        }, result);
        for(std::size_t i = reduction_info.back().preimages_start; i < strong_stack.size(); ++i) {
          arena.drop(std::move(strong_stack[i]));
        }
        strong_stack.erase(strong_stack.begin() + reduction_info.back().preimages_start, strong_stack.end());
        /*
          Now we move down the stack and pass the result to the last listener
        */
        if(resumptions.empty()) {
          if(strong_stack.size() != 1) std::terminate(); //unreachable
          return std::move(strong_stack[0]);
        } else {
          reduction_info.pop_back();
          last_result = config.resume(context, std::move(resumptions.back()), result);
          resumptions.pop_back();
          goto HANDLE_RESULT;
        }
      } else if(auto* reduced = std::get_if<reduction_step_result::Reduced>(&last_result)) {
        if(strong_stack.size() != reduction_info.back().preimages_end) std::terminate(); //preimages should be on top of stack at this point
        strong_stack.push_back(std::move(strong_stack[reduction_info.back().reduction_index])); //push old value as a new preimage.
        ++reduction_info.back().preimages_end; //remember that we pushed the new preimage
        strong_stack[reduction_info.back().reduction_index] = std::move(reduced->result);
        if(reduced->should_reduce_more) {
          goto START_REDUCE_TOP;
        } else {
          last_result = reduction_step_result::Finished{};
          goto HANDLE_RESULT;
        }
      } else {
        auto& request = std::get<2>(last_result);
        resumptions.push_back(std::move(request.then));
        reduction_info.push_back(get_reduction_info_for(request.strong_stack_index));
        goto START_REDUCE_TOP;
      }
    };
  }
  template<class ReductionConfiguration>
  OwnedExpression reduce_generic(Arena& arena, OwnedExpression expr, ReductionConfiguration config) {
    return reduce_generic(arena, std::move(expr), std::move(config), TrivialMemoizer{});
  }
  SimpleEvaluationContext::SimpleEvaluationContext(Arena& arena, RuleCollector& rule_collector):arena(&arena), rule_collector(&rule_collector) {}
  OwnedExpression SimpleEvaluationContext::reduce_head(OwnedExpression expr) {
    return reduce_generic(*arena, std::move(expr), ReductionConfiguration{*rule_collector}, WeakKeyMemoizer{*arena});
  }
  OwnedExpression SimpleEvaluationContext::reduce(OwnedExpression expr) {
    return reduce_generic(
      *arena,
      std::move(expr),
      PipelineConfig{
        ReductionConfiguration{*rule_collector},
        ArgumentReducerConfig{}
      },
      WeakKeyMemoizer{*arena}
    );
  }
  namespace {
    namespace conglomerate_status {
      struct PureOpen {
        OwnedExpression representative; //some expression equal to the conglomerate (not itself including conglomerates)
        std::size_t conglomerate_index; //index of actual associated conglomerate
        std::vector<std::size_t> application_conglomerates; //any conglomerate with a reducer like "#0 x"
        static constexpr auto part_info = mdb::parts::simple<3>;
      };
      struct Axiomatic {
        OwnedExpression head;
        std::vector<std::size_t> applied_conglomerates;
        std::size_t conglomerate_index;
        static constexpr auto part_info = mdb::parts::simple<3>;
      };
      using Any = std::variant<PureOpen, Axiomatic>;
    };
    struct ConglomerateReducer {
      std::vector<OwnedExpression> active; //these must be pure open expressions
      std::vector<OwnedExpression> waiting;
      static constexpr auto part_info = mdb::parts::simple<2>;
    };
    struct ConglomerateInfo {
      ConglomerateReducer reducers;
      conglomerate_status::Any status;
      static constexpr auto part_info = mdb::parts::simple<2>;
    };
    struct ConglomerateSolveState {
      Arena& arena;
      RuleCollector const& rule_collector;
      std::vector<ConglomerateInfo> conglomerate_class_info;
      std::vector<std::size_t> conglomerate_to_class;
      static constexpr auto part_info = mdb::parts::simple<4>;
      template<class Updater>
      void update_class_indices(Updater&& update_class_index) {
        for(auto& i : conglomerate_to_class) {
          i = update_class_index(i);
        }
        for(auto& conglomerate_class : conglomerate_class_info) {
          std::visit(mdb::overloaded{
            [&](conglomerate_status::Axiomatic& axiomatic) {
              for(auto& i : axiomatic.applied_conglomerates) {
                i = update_class_index(i);
              }
            },
            [&](conglomerate_status::PureOpen& pure_open) {
              for(auto& i : pure_open.application_conglomerates) {
                i = update_class_index(i);
              }
            }
          }, conglomerate_class.status);
        }
      }
      OwnedExpression eliminate_conglomerates(WeakExpression);
      bool exists_axiomatic_cycle() {
        constexpr std::uint8_t on_stack = 1;
        constexpr std::uint8_t acyclic = 2;
        struct Detail {
          std::vector<ConglomerateInfo> const& conglomerate_class_info;
          std::vector<std::uint8_t> node_info;
          Detail(std::vector<ConglomerateInfo> const& conglomerate_class_info):
            conglomerate_class_info(conglomerate_class_info),
            node_info(conglomerate_class_info.size(), 0){}
          bool has_cycle(std::size_t i) {
            //this is essentially Tarjan's algorithm, except we only care whether a cycle exists.
            if(node_info[i] == acyclic) return false;
            if(node_info[i] == on_stack) return true;
            node_info[i] = on_stack;
            bool subcycle = std::visit(mdb::overloaded{
              [&](conglomerate_status::Axiomatic const& axiomatic) {
                for(auto applied : axiomatic.applied_conglomerates) {
                  if(has_cycle(applied)) {
                    return true;
                  }
                }
                return false;
              },
              [&](conglomerate_status::PureOpen const& pure_open) {
                for(auto applied : pure_open.application_conglomerates) {
                  if(has_cycle(applied)) {
                    return true;
                  }
                }
                return false;
              }
            }, conglomerate_class_info[i].status);
            if(subcycle) return true;
            node_info[i] = acyclic;
            return false;
          }
        };
        Detail detail{conglomerate_class_info};
        for(std::size_t i = 0; i < conglomerate_class_info.size(); ++i) {
          if(detail.has_cycle(i)) {
            return true;
          }
        }
        return false;
      }
      bool combine_conglomerates(std::size_t target, std::size_t source) {
        struct Detail {
          ConglomerateSolveState& me;
          std::vector<std::pair<std::size_t, std::size_t> > further_combines;
          bool combine(std::size_t target, std::size_t source) {
            if(target == source) return true;
            if(target > source) std::swap(target, source); //swap into lower index to maintain representative invariant
            auto& target_class = me.conglomerate_class_info[target];
            auto& source_class = me.conglomerate_class_info[source];
            for(auto& active : source_class.reducers.active) {
              target_class.reducers.active.push_back(std::move(active));
            }
            for(auto& waiting : source_class.reducers.waiting) {
              target_class.reducers.waiting.push_back(std::move(waiting));
            }
            if(auto* source_axiomatic = std::get_if<conglomerate_status::Axiomatic>(&source_class.status)) {
              if(auto* target_axiomatic = std::get_if<conglomerate_status::Axiomatic>(&target_class.status)) {
                if(source_axiomatic->head != target_axiomatic->head) {
                  return false;
                }
                if(source_axiomatic->applied_conglomerates.size() != target_axiomatic->applied_conglomerates.size()) {
                  return false;
                }
                me.arena.drop(std::move(target_axiomatic->head)); //target will be destroyed
                for(std::size_t i = 0; i < source_axiomatic->applied_conglomerates.size(); ++i) {
                  further_combines.emplace_back(
                    source_axiomatic->applied_conglomerates[i],
                    target_axiomatic->applied_conglomerates[i]
                  );
                }
              } else {
                me.arena.drop(std::move(std::get<0>(target_class.status).representative));
                target_class.status = std::move(*source_axiomatic);
              }
            } else if(auto* target_pure_open = std::get_if<conglomerate_status::PureOpen>(&target_class.status)) {
              //merge application_conglomerates
              auto& source_pure_open = std::get<conglomerate_status::PureOpen>(source_class.status);
              me.arena.drop(std::move(source_pure_open.representative));
              target_pure_open->application_conglomerates.insert(
                target_pure_open->application_conglomerates.end(),
                source_pure_open.application_conglomerates.begin(),
                source_pure_open.application_conglomerates.end()
              );
              std::sort(target_pure_open->application_conglomerates.begin(), target_pure_open->application_conglomerates.end());
              auto unique_end = std::unique(target_pure_open->application_conglomerates.begin(), target_pure_open->application_conglomerates.end());
              target_pure_open->application_conglomerates.erase(unique_end, target_pure_open->application_conglomerates.end());
            } else {
              me.arena.drop(std::move(std::get<0>(source_class.status).representative));
            }
            me.conglomerate_class_info.erase(me.conglomerate_class_info.begin() + source);
            auto new_index = [&](std::size_t index) {
              if(index < source) {
                return index;
              } else if(index == source) {
                return target;
              } else {
                return index - 1;
              }
            };
            me.update_class_indices(new_index);
            for(auto& entry : further_combines) {
              entry.first = new_index(entry.first);
              entry.second = new_index(entry.second);
            }
            return true;
          }
        };
        Detail detail{*this};
        detail.further_combines.emplace_back(source, target);
        while(!detail.further_combines.empty()) {
          auto [top_source, top_target] = detail.further_combines.back();
          detail.further_combines.pop_back();
          if(!detail.combine(top_source, top_target)) return false;
        }
        return !exists_axiomatic_cycle(); //succeeds so long as no cycle is created.
      }
      std::size_t create_conglomerate(std::vector<OwnedExpression> representatives) { //must be *non-empty* vector.
        auto class_index = conglomerate_class_info.size();
        auto conglomerate_index = conglomerate_to_class.size();
        WeakExpression representative_weak = representatives[0];
        conglomerate_class_info.push_back({
          .reducers = {
            .waiting = std::move(representatives)
          },
          .status = conglomerate_status::PureOpen{
            .representative = eliminate_conglomerates(representative_weak),
            .conglomerate_index = conglomerate_index
          }
        });
        conglomerate_to_class.push_back(class_index);
        return class_index;
      }
      void explode_conglomerate(std::size_t target_class, OwnedExpression head, std::vector<OwnedExpression> args) {
        std::vector<std::size_t> applied_conglomerates;
        std::transform(args.begin(), args.end(), std::back_inserter(applied_conglomerates), [&](OwnedExpression& arg) {
          return create_conglomerate(mdb::make_vector(std::move(arg)));
        });
        if(conglomerate_class_info[target_class].status.index() == 1) std::terminate(); //can only explode things once
        arena.drop(std::move(std::get<0>(conglomerate_class_info[target_class].status).representative));
        conglomerate_class_info[target_class].status = conglomerate_status::Axiomatic{
          .head = std::move(head),
          .applied_conglomerates = std::move(applied_conglomerates),
          .conglomerate_index = std::get<0>(conglomerate_class_info[target_class].status).conglomerate_index
        };
      }
      bool has_computation(WeakExpression expr); //if *any* pattern could possibly reduce expr (possibly with more args)
      void drop_reducer(std::size_t target_class, std::size_t target_reducer) {
        arena.drop(std::move(conglomerate_class_info[target_class].reducers.active[target_reducer]));
        conglomerate_class_info[target_class].reducers.active.erase(conglomerate_class_info[target_class].reducers.active.begin() + target_reducer);
      }
      bool examine_updated_reducer(std::size_t target_class, std::size_t target_reducer) { //must be in "active" row - but presumably ignored in reduction. must also be reduced
        WeakExpression reducer = conglomerate_class_info[target_class].reducers.active[target_reducer];
        if(auto* conglomerate = arena.get_if_conglomerate(reducer)) {
          drop_reducer(target_class, target_reducer);
          return combine_conglomerates(target_class, conglomerate_to_class[conglomerate->index]);
        }
        auto unfolding = unfold(arena, reducer);
        if(arena.holds_axiom(unfolding.head) || arena.holds_data(unfolding.head)) {
          if(auto* axiomatic = std::get_if<conglomerate_status::Axiomatic>(&conglomerate_class_info[target_class].status)) {
            if(axiomatic->head == unfolding.head && axiomatic->applied_conglomerates.size() == unfolding.args.size()) {
              for(std::size_t i = 0; i < unfolding.args.size(); ++i) {
                conglomerate_class_info[axiomatic->applied_conglomerates[i]].reducers.waiting.push_back(arena.copy(unfolding.args[i]));
              }
              drop_reducer(target_class, target_reducer);
              return true;
            } else {
              return false; //mismatched axioms
            }
          } else {
            std::vector<OwnedExpression> args;
            std::transform(unfolding.args.begin(), unfolding.args.end(), std::back_inserter(args), [&](WeakExpression expr) {
              return arena.copy(expr);
            });
            explode_conglomerate(target_class, arena.copy(unfolding.head), std::move(args));
            drop_reducer(target_class, target_reducer);
            return true;
          }
        } else if(auto* base_conglomerate = arena.get_if_conglomerate(unfolding.head)) {
          auto base_index = conglomerate_to_class[base_conglomerate->index];
          auto& base_status = std::get<conglomerate_status::PureOpen>(conglomerate_class_info[base_index].status); //only pure opens can appear as conglomerates
          base_status.application_conglomerates.push_back(target_class);
          return !exists_axiomatic_cycle();
        } else if(has_computation(reducer)) {
          return false; //no good - can't deal with expressions with computations
        } else {
          return true; //okay - still purely open
        }
      }
      std::size_t conglomerate_index_of_class(std::size_t class_index) {
        return std::visit([](auto const& status) {
          return status.conglomerate_index;
        }, conglomerate_class_info[class_index].status);
      }
      bool conglomerate_has_definition(std::size_t index) {
        return std::visit(mdb::overloaded{
          [&](conglomerate_status::Axiomatic const& axiomatic) {
            return true;
          },
          [&](conglomerate_status::PureOpen const& pure_open) {
            return pure_open.conglomerate_index != index; //need to redirect to another representative
          }
        }, conglomerate_class_info[conglomerate_to_class[index]].status);
      }
      OwnedExpression get_conglomerate_class_replacement(std::size_t index) {
        return std::visit(mdb::overloaded{
          [&](conglomerate_status::Axiomatic const& axiomatic) {
            OwnedExpression ret = arena.copy(axiomatic.head);
            for(std::size_t conglomerate_class : axiomatic.applied_conglomerates) {
              ret = arena.apply(
                std::move(ret),
                arena.conglomerate(conglomerate_index_of_class(conglomerate_class))
              );
            }
            return ret;
          },
          [&](conglomerate_status::PureOpen const& pure_open) {
            return arena.conglomerate(pure_open.conglomerate_index);
          }
        }, conglomerate_class_info[index].status);
      }
      OwnedExpression get_conglomerate_replacement(std::size_t index) {
        return get_conglomerate_class_replacement(conglomerate_to_class[index]);
      }
      OwnedExpression get_conglomerate_class_elimination_replacement(std::size_t index) {
        return std::visit(mdb::overloaded{
          [&](conglomerate_status::Axiomatic const& axiomatic) {
            OwnedExpression ret = arena.copy(axiomatic.head);
            for(std::size_t conglomerate_class : axiomatic.applied_conglomerates) {
              ret = arena.apply(
                std::move(ret),
                get_conglomerate_class_elimination_replacement(conglomerate_class)
              );
            }
            return ret;
          },
          [&](conglomerate_status::PureOpen const& pure_open) {
            return arena.copy(pure_open.representative);
          }
        }, conglomerate_class_info[index].status);
      }
      OwnedExpression get_conglomerate_elimination_replacement(std::size_t index) {
        return get_conglomerate_class_elimination_replacement(conglomerate_to_class[index]);
      }
      AssumptionInfo list_assumptions() {
        AssumptionInfo ret;
        for(std::size_t class_index = 0; class_index < conglomerate_class_info.size(); ++class_index) {
          auto const& c = conglomerate_class_info[class_index];
          if(c.reducers.active.empty()) continue; //ignore empty classes.
          ret.assumptions.push_back({
            .representative = get_conglomerate_class_replacement(class_index),
            .terms = copy_on_arena(arena, c.reducers.active)
          });
        }
        return ret;
      };
      std::optional<PartialMap> try_to_map_to(EvaluationContext& target, MapRequest request) {
        struct Detail {
          ConglomerateSolveState& me;
          EvaluationContext& target;
          PartialMap ret;
          std::vector<MapRequestConstraint> unchecked_constraints;
          std::vector<MapRequestConstraint> map_only_constraints;
          std::vector<std::size_t> waiting_conglomerates;
          std::vector<std::pair<std::size_t, std::size_t> > remaining_class_constraints;
          std::vector<std::pair<OwnedExpression, std::size_t> > waiting_match_mapped_expression_to_class;
          Detail(ConglomerateSolveState& me, EvaluationContext& target, MapRequest request):me(me), target(target), unchecked_constraints(std::move(request.constraints)) {
            for(auto& constraint : unchecked_constraints) {
              constraint.target = target.reduce(std::move(constraint.target));
            }
            for(std::size_t class_index = 0; class_index < me.conglomerate_class_info.size(); ++class_index) {
              for(std::size_t reducer_index = 0; reducer_index < me.conglomerate_class_info[class_index].reducers.active.size(); ++reducer_index) {
                remaining_class_constraints.emplace_back(class_index, reducer_index);
              }
            }
            for(std::size_t conglomerate = 0; conglomerate < me.conglomerate_to_class.size(); ++conglomerate) {
              if(me.conglomerate_index_of_class(me.conglomerate_to_class[conglomerate]) == conglomerate) continue; //don't worry about principal representatives
              waiting_conglomerates.push_back(conglomerate);
            }
          }
          //This function should be called to express that we want the mapping #<class_index> -> mapped_expr.
          //mapped_expr should be in the codomain and already reduced.
          bool match_mapped_expression_to_class(OwnedExpression mapped_expr, std::size_t class_index) {
            auto const& class_info = me.conglomerate_class_info[class_index];
            auto class_representative = me.conglomerate_index_of_class(class_index);
            RAIIDestroyer mapped_destroyer{me.arena, mapped_expr};
            if(ret.conglomerate_map.contains(class_representative)) {
              return ret.conglomerate_map.at(class_representative) == mapped_expr;
            } else {
              ret.conglomerate_map.insert(std::make_pair(class_representative, me.arena.copy(mapped_expr)));
              if(auto const* axiomatic = std::get_if<conglomerate_status::Axiomatic>(&class_info.status)) {
                auto unfolded = unfold(me.arena, mapped_expr);
                if(unfolded.head != axiomatic->head || unfolded.args.size() != axiomatic->applied_conglomerates.size()) {
                  return false;
                }
                for(std::size_t i = 0; i < unfolded.args.size(); ++i) {
                  waiting_match_mapped_expression_to_class.emplace_back(me.arena.copy(unfolded.args[i]), axiomatic->applied_conglomerates[i]);
                }
              }
              return true;
            }
          }
          enum class ProgressResult {
            nothing,
            finished,
            failed
          };
          ProgressResult check_map_constraint(MapRequestConstraint& constraint) { //destroys constraint if it makes progress
            if(ret.can_map(me.arena, constraint.source)) {
              auto output = target.reduce(ret.map(me.arena, constraint.source));
              RAIIDestroyer destroyer{me.arena, output};
              if(output == constraint.target) {
                destroy_from_arena(me.arena, constraint);
                return ProgressResult::finished;
              } else {
                destroy_from_arena(me.arena, constraint);
                return ProgressResult::failed;
              }
            }
            return ProgressResult::nothing;
          }
          bool check_initial_constraint(MapRequestConstraint&& constraint) { //return false for failure. should consume request
            auto map_result = check_map_constraint(constraint);
            if(map_result == ProgressResult::finished) return true;
            if(map_result == ProgressResult::failed) return false;
            if(auto* arg = me.arena.get_if_argument(constraint.source)) {
              ret.arg_map.insert(std::make_pair(arg->index, me.arena.copy(constraint.target)));
              destroy_from_arena(me.arena, constraint);
              return true;
            } else if(auto* conglomerate = me.arena.get_if_conglomerate(constraint.source)) {
              waiting_match_mapped_expression_to_class.emplace_back(me.arena.copy(constraint.target), me.conglomerate_to_class[conglomerate->index]);
              destroy_from_arena(me.arena, constraint);
              return true;
            } else {
              auto unfolded = unfold(me.arena, constraint.source);
              if(me.arena.holds_axiom(unfolded.head) || me.arena.holds_data(unfolded.head)) {
                auto unfolded_target = unfold(me.arena, constraint.target);
                if(unfolded.head != unfolded_target.head || unfolded.args.size() != unfolded_target.args.size()) {
                  destroy_from_arena(me.arena, constraint);
                  return false;
                }
                for(std::size_t i = 0; i < unfolded.args.size(); ++i) {
                  unchecked_constraints.push_back({
                    .source = me.arena.copy(unfolded.args[i]),
                    .target = me.arena.copy(unfolded_target.args[i])
                  });
                }
                destroy_from_arena(me.arena, constraint);
                return true;
              } else {
                map_only_constraints.push_back(std::move(constraint));
                return true;
              }
            }
          }
          ProgressResult check_reducer(std::size_t class_index, std::size_t reducer_index) {
            auto const& reducer = me.conglomerate_class_info[class_index].reducers.active[reducer_index];
            auto class_representative = me.conglomerate_index_of_class(class_index);
            if(ret.can_map(me.arena, reducer)) {
              auto output = target.reduce(ret.map(me.arena, reducer));
              waiting_match_mapped_expression_to_class.emplace_back(std::move(output), class_index);
              return ProgressResult::finished;
            } else if(ret.conglomerate_map.contains(class_representative)) {
              if(auto* arg = me.arena.get_if_argument(reducer)) {
                ret.arg_map.insert(std::make_pair(arg->index, me.arena.copy(ret.conglomerate_map.at(class_representative))));
                return ProgressResult::finished;
              }
            }
            return ProgressResult::nothing;
          }
          bool run() {
            bool made_progress = true;
            bool failed = false;
            while(made_progress && !failed) {
              made_progress = false;
              while(!unchecked_constraints.empty()) {
                made_progress = true;
                MapRequestConstraint constraint = std::move(unchecked_constraints.back());
                unchecked_constraints.pop_back();
                if(!check_initial_constraint(std::move(constraint))) return false;
              }
              while(!waiting_match_mapped_expression_to_class.empty()) {
                made_progress = true;
                auto back = std::move(waiting_match_mapped_expression_to_class.back());
                waiting_match_mapped_expression_to_class.pop_back();
                if(!match_mapped_expression_to_class(std::move(back.first), back.second)) return false;
              }
              mdb::erase_from_active_queue(remaining_class_constraints, [&](auto const& pair) {
                auto ret = check_reducer(pair.first, pair.second);
                if(ret == ProgressResult::failed) failed = true;
                if(ret == ProgressResult::finished) made_progress = true;
                return ret != ProgressResult::nothing;
              });
              mdb::erase_from_active_queue(map_only_constraints, [&](auto& constraint) {
                auto ret = check_map_constraint(constraint);
                if(ret == ProgressResult::failed) failed = true;
                if(ret == ProgressResult::finished) made_progress = true;
                return ret != ProgressResult::nothing;
              });
              mdb::erase_from_active_queue(waiting_conglomerates, [&](std::size_t conglomerate) { //propagate classes of conglomerates
                auto class_index = me.conglomerate_to_class[conglomerate];
                auto class_representative = me.conglomerate_index_of_class(class_index);
                if(ret.conglomerate_map.contains(class_representative)) {
                  if(ret.conglomerate_map.contains(conglomerate)) {
                    std::terminate(); //unreachable - no one sets these entries on non-principal conglomerates.
                  } else {
                    ret.conglomerate_map.insert(std::make_pair(conglomerate, me.arena.copy(ret.conglomerate_map.at(class_representative))));
                    return true;
                  }
                }
                return false;
              });
            }
            return !failed && map_only_constraints.empty();
          }
        };
        Detail detail{*this, target, std::move(request)};
        if(detail.run()) {
          return std::move(detail.ret);
        } else {
          destroy_from_arena(arena, detail.ret, detail.unchecked_constraints, detail.map_only_constraints, detail.waiting_match_mapped_expression_to_class);
          return std::nullopt;
        }
      }

    };
    template<class Base>
    struct ConglomerateReducerCRTP : ReducerCRTP<ConglomerateReducerCRTP<Base> > {
      using Mark = std::bitset<64>;
      Arena& arena;
      RuleCollector const& rule_collector;
      WeakKeyMap<OwnedExpression, PartDestroyer> reductions;
      ConglomerateReducerCRTP(Arena& arena, RuleCollector const& rule_collector):arena(arena), rule_collector(rule_collector), reductions(arena) {}
      Base& me() { return *(Base*)this; }
      using ReducerCRTP<ConglomerateReducerCRTP<Base> >::reduce_by_pattern;
      OwnedExpression reduce(OwnedExpression expr, bool outermost = false) {
        if(reductions.contains(expr)) {
          auto ret = arena.copy(reductions.at(expr));
          arena.drop(std::move(expr));
          return ret;
        }
        WeakExpression input = expr;
        bool needs_repeat = true;
        while(needs_repeat) {
          needs_repeat = false;
          while(reduce_by_pattern(expr, rule_collector));
          auto unfolded = unfold_owned(arena, std::move(expr));
          for(auto& arg : unfolded.args) {
            arg = reduce(std::move(arg));
          }
          while(true) {
            if(auto reduction = me().get_conglomerate_reduction(unfolded.head, outermost && unfolded.args.empty())) {
              arena.drop(std::move(unfolded.head));
              unfolded.head = std::move(*reduction);
              needs_repeat = true;
            }
            if(unfolded.args.empty()) break;
            unfolded.head = arena.apply(
              std::move(unfolded.head),
              std::move(unfolded.args.front())
            );
            unfolded.args.erase(unfolded.args.begin());
          }
          expr = std::move(unfolded.head);
        }
        reductions.set(input, arena.copy(expr));
        return std::move(expr);
      }
      OwnedExpression reduce_outer(OwnedExpression expr) {
        return reduce(std::move(expr), true);
      }
    };
    struct RepresentativeReducer : ConglomerateReducerCRTP<RepresentativeReducer> {
      ConglomerateSolveState& solve_state;
      std::size_t target_class;
      std::size_t target_index;
      RepresentativeReducer(Arena& arena, RuleCollector const& rule_collector, ConglomerateSolveState& solve_state,
                            std::size_t target_class, std::size_t target_index):
                            ConglomerateReducerCRTP<RepresentativeReducer>(arena, rule_collector),
                            solve_state(solve_state),
                            target_class(target_class),
                            target_index(target_index){}
      std::optional<OwnedExpression> get_conglomerate_reduction(WeakExpression expr, bool outermost) {
        if(auto* conglomerate = arena.get_if_conglomerate(expr)) {
          if(outermost) return std::nullopt; //don't expand raw conglomerates
          if(solve_state.conglomerate_has_definition(conglomerate->index)) {
            return solve_state.get_conglomerate_replacement(conglomerate->index);
          } else {
            return std::nullopt; //conglomerates can't be reducers
          }
        }
        for(std::size_t class_index = 0; class_index < solve_state.conglomerate_class_info.size(); ++class_index) {
          auto& class_info = solve_state.conglomerate_class_info[class_index];
          for(std::size_t reducer_index = 0; reducer_index < class_info.reducers.active.size(); ++reducer_index) {
            if(class_index == target_class && reducer_index == target_index) continue; //don't use a reducer on itself.
            auto& reducer = class_info.reducers.active[reducer_index];
            if(expr == reducer) {
              if(outermost) {
                //substitutions at root need to go to the raw conglomerate.
                return arena.conglomerate(solve_state.conglomerate_index_of_class(class_index));
              } else {
                return solve_state.get_conglomerate_class_replacement(class_index);
              }
            }
          }
        }
        return std::nullopt;
      }
    };

    struct SeriouslyNormalConglomerateReducer {
      Arena& arena;
      RuleCollector const& rule_collector;
      ConglomerateSolveState& solve_state;
      std::optional<OwnedExpression> get_conglomerate_reduction(WeakExpression expr) {
        if(auto* conglomerate = arena.get_if_conglomerate(expr)) {
          if(solve_state.conglomerate_has_definition(conglomerate->index)) {
            return solve_state.get_conglomerate_replacement(conglomerate->index);
          } else {
            return std::nullopt; //conglomerates can't be reducers
          }
        }
        for(std::size_t class_index = 0; class_index < solve_state.conglomerate_class_info.size(); ++class_index) {
          auto& class_info = solve_state.conglomerate_class_info[class_index];
          for(std::size_t reducer_index = 0; reducer_index < class_info.reducers.active.size(); ++reducer_index) {
            auto& reducer = class_info.reducers.active[reducer_index];
            if(expr == reducer) {
              return solve_state.get_conglomerate_class_replacement(class_index);
            }
          }
        }
        return std::nullopt;
      }

    };
    std::optional<OwnedExpression> get_normal_conglomerate_reduction(Arena& arena, ConglomerateSolveState& solve_state, WeakExpression expr) {
      if(auto* conglomerate = arena.get_if_conglomerate(expr)) {
        if(solve_state.conglomerate_has_definition(conglomerate->index)) {
          return solve_state.get_conglomerate_replacement(conglomerate->index);
        } else {
          return std::nullopt; //conglomerates can't be reducers
        }
      }
      for(std::size_t class_index = 0; class_index < solve_state.conglomerate_class_info.size(); ++class_index) {
        auto& class_info = solve_state.conglomerate_class_info[class_index];
        for(std::size_t reducer_index = 0; reducer_index < class_info.reducers.active.size(); ++reducer_index) {
          auto& reducer = class_info.reducers.active[reducer_index];
          if(expr == reducer) {
            return solve_state.get_conglomerate_class_replacement(class_index);
          }
        }
      }
      return std::nullopt;
    }
    struct ConglomerateReducerState{};
    struct ConglomerateReducerConfig {
      using ResumptionKind = ConglomerateReducerState;
      using Result = reduction_step_result::Any<ConglomerateReducerState>;
      ConglomerateSolveState& solve_state;
      Result reduce(ReduceByPatternContext context, WeakExpression expr) {
        if(auto reduction = get_normal_conglomerate_reduction(context.arena, solve_state, expr)) {
          return reduction_step_result::Reduced{
            .result = std::move(*reduction)
          };
        } else {
          return reduction_step_result::Finished{};
        }
      }
      Result resume(ReduceByPatternContext context, ConglomerateReducerState, WeakExpression) {
        std::terminate(); //unreachable
      }
    };
    struct NormalConglomerateReducer : ConglomerateReducerCRTP<NormalConglomerateReducer> {
      ConglomerateSolveState& solve_state;
      NormalConglomerateReducer(Arena& arena, RuleCollector const& rule_collector, ConglomerateSolveState& solve_state):
        ConglomerateReducerCRTP<NormalConglomerateReducer>(arena, rule_collector),
        solve_state(solve_state){}
      std::optional<OwnedExpression> get_conglomerate_reduction(WeakExpression expr, bool) {
        return get_normal_conglomerate_reduction(arena, solve_state, expr);
      }
      bool is_lambda_like(WeakExpression expr) {
        auto unfolded = unfold(arena, expr);
        std::size_t max_args = 0;
        if(arena.holds_declaration(unfolded.head)) {
          auto const& declaration_info = rule_collector.declaration_info(unfolded.head);
          for(auto const& rule : declaration_info.rules) {
            if(rule.pattern_body.args_captured > max_args) {
              max_args = rule.pattern_body.args_captured;
            }
          }
        }
        if(max_args > unfolded.args.size()) {
          auto owned = arena.copy(expr);
          auto blank_axiom = arena.axiom();
          for(std::size_t i = 0; i < max_args - unfolded.args.size(); ++i) {
            owned = arena.apply(
              std::move(owned),
              arena.copy(blank_axiom)
            );
          }
          auto ret = reduce_by_pattern(owned, rule_collector);
          destroy_from_arena(arena, owned, blank_axiom);
          return ret;
        } else {
          return false;
        }
      }
    };
    struct IndeterminateDetector { //holy code duplication, batman!
      Arena& arena;
      RuleCollector const& collector;
      ConglomerateSolveState& solve_state;
      OwnedExpression indeterminate_head; //special declaration
      WeakKeyMap<OwnedExpression, PartDestroyer> reductions;
      IndeterminateDetector(Arena& arena, RuleCollector const& collector, ConglomerateSolveState& solve_state)
        :arena(arena),
        collector(collector),
        solve_state(solve_state),
        indeterminate_head(arena.declaration()),
        reductions(arena){}
      ~IndeterminateDetector() {
        arena.drop(std::move(indeterminate_head));
      }
      OwnedExpression reduce(OwnedExpression, bool);
      bool reduce_by_pattern(OwnedExpression& expr, bool consider_extra_args = false) { // return true if it should be called again.
        auto unfolded = unfold(arena, expr);
        if(unfolded.head == indeterminate_head) {
          arena.drop(std::move(expr));
          expr = arena.copy(indeterminate_head);
          return false;
        } else if(arena.holds_declaration(unfolded.head)) {
          auto const& declaration_info = collector.declaration_info(unfolded.head);
          for(auto const& rule : declaration_info.rules) {
            if(consider_extra_args || rule.pattern_body.args_captured <= unfolded.args.size()) {
              bool is_indeterminate = false;
              std::vector<WeakExpression> pattern_stack;
              auto next_arg = unfolded.args.begin();
              std::vector<OwnedExpression> novel_roots; //storage for new expressions we create
              for(auto const& step : rule.pattern_body.steps) {
                if(auto* match = std::get_if<PatternMatch>(&step)) {
                  auto new_expr = substitute_into(arena, match->substitution, mdb::as_span(pattern_stack));
                  new_expr = reduce(std::move(new_expr), false);
                  auto match_unfold = unfold(arena, new_expr);
                  novel_roots.push_back(std::move(new_expr)); //keep reference for later deletion
                  if(new_expr == indeterminate_head) {
                    is_indeterminate = true;
                    for(std::size_t i = 0; i < match->args_captured; ++i) {
                      pattern_stack.push_back(indeterminate_head);
                    }
                  } else {
                    if(match_unfold.head != match->expected_head) goto PATTERN_FAILED;
                    if(match_unfold.args.size() != match->args_captured) goto PATTERN_FAILED;
                    pattern_stack.insert(pattern_stack.end(), match_unfold.args.begin(), match_unfold.args.end());
                  }
                } else if(auto* data_check = std::get_if<DataCheck>(&step)) {
                  auto new_expr = arena.copy(pattern_stack[data_check->capture_index]);
                  new_expr = reduce(std::move(new_expr), false);
                  pattern_stack[data_check->capture_index] = new_expr;
                  bool success = [&] {
                    if(auto* data = arena.get_if_data(new_expr)) {
                      return data->type_index == data_check->expected_type;
                    } else {
                      return is_indeterminate = new_expr == indeterminate_head;
                    }
                  }();
                  novel_roots.push_back(std::move(new_expr));
                  if(!success) goto PATTERN_FAILED;
                } else {
                  if(!std::holds_alternative<PullArgument>(step)) std::terminate(); //make sure we hit every case
                  if(next_arg != unfolded.args.end()) {
                    pattern_stack.push_back(*next_arg);
                    ++next_arg;
                  } else {
                    pattern_stack.push_back(indeterminate_head);
                    is_indeterminate = true;
                  }
                }
              }
              //if we get here, the pattern succeeded.
              if(is_indeterminate) {
                arena.drop(std::move(expr));
                destroy_from_arena(arena, novel_roots);
                expr = arena.copy(indeterminate_head);
                return false;
              } else {
                auto new_expr = std::visit(mdb::overloaded{
                  [&](OwnedExpression const& expr) {
                    return substitute_into(arena, expr, mdb::as_span(pattern_stack));
                  },
                  [&](mdb::function<OwnedExpression(std::span<WeakExpression>)> const& func) {
                    return func(mdb::as_span(pattern_stack));
                  }
                }, rule.replacement);
                arena.drop(std::move(expr));
                expr = std::move(new_expr);
                destroy_from_arena(arena, novel_roots);
                return true;
              }
            PATTERN_FAILED:
              destroy_from_arena(arena, novel_roots);
            }
          }
        }
        return false;
      }
      std::optional<OwnedExpression> get_conglomerate_reduction(WeakExpression expr) {
        if(auto* conglomerate = arena.get_if_conglomerate(expr)) {
          if(solve_state.conglomerate_has_definition(conglomerate->index)) {
            return solve_state.get_conglomerate_replacement(conglomerate->index);
          } else {
            return std::nullopt; //conglomerates can't be reducers
          }
        }
        for(std::size_t class_index = 0; class_index < solve_state.conglomerate_class_info.size(); ++class_index) {
          auto& class_info = solve_state.conglomerate_class_info[class_index];
          for(std::size_t reducer_index = 0; reducer_index < class_info.reducers.active.size(); ++reducer_index) {
            auto& reducer = class_info.reducers.active[reducer_index];
            if(expr == reducer) {
              return solve_state.get_conglomerate_class_replacement(class_index);
            }
          }
        }
        return std::nullopt;
      }
    };
    OwnedExpression IndeterminateDetector::reduce(OwnedExpression expr, bool outer) {
      if(reductions.contains(expr)) {
        auto ret = arena.copy(reductions.at(expr));
        arena.drop(std::move(expr));
        return ret;
      }
      WeakExpression input = expr;
      bool needs_repeat = true;
      while(needs_repeat) {
        needs_repeat = false;
        while(reduce_by_pattern(expr, outer));
        auto unfolded = unfold_owned(arena, std::move(expr));
        for(auto& arg : unfolded.args) {
          arg = reduce(std::move(arg), false);
        }
        while(true) {
          if(auto reduction = get_conglomerate_reduction(unfolded.head)) {
            arena.drop(std::move(unfolded.head));
            unfolded.head = std::move(*reduction);
            needs_repeat = true;
          }
          if(unfolded.args.empty()) break;
          unfolded.head = arena.apply(
            std::move(unfolded.head),
            std::move(unfolded.args.front())
          );
          unfolded.args.erase(unfolded.args.begin());
        }
        expr = std::move(unfolded.head);
      }
      reductions.set(input, arena.copy(expr));
      return std::move(expr);
    }
    bool ConglomerateSolveState::has_computation(WeakExpression expr) {
      IndeterminateDetector detector{arena, rule_collector, *this};
      auto out = detector.reduce(arena.copy(expr), true);
      auto ret = out == detector.indeterminate_head;
      arena.drop(std::move(out));
      return ret;
    }
    OwnedExpression ConglomerateSolveState::eliminate_conglomerates(WeakExpression expr) {
      return arena.visit(expr, mdb::overloaded{
        [&](Apply const& apply) -> OwnedExpression {
          return arena.apply(
            eliminate_conglomerates(apply.lhs),
            eliminate_conglomerates(apply.rhs)
          );
        },
        [&](Axiom const&) { return arena.copy(expr); },
        [&](Declaration const&) { return arena.copy(expr); },
        [&](Argument const&) { return arena.copy(expr); },
        [&](Conglomerate const& conglomerate) -> OwnedExpression  {
          return get_conglomerate_elimination_replacement(conglomerate.index);
        },
        [&](Data const& data) {
          return arena.modify_subexpressions_of(data, [&](WeakExpression subexpr) {
            return eliminate_conglomerates(subexpr);
          });
        }
      });
    }
  }
  struct EvaluationContext::Impl {
    Arena& arena;
    RuleCollector const& rule_collector;
    ConglomerateSolveState solve_state;
    Impl(Arena& arena, RuleCollector const& rule_collector):arena(arena), rule_collector(rule_collector), solve_state{.arena = arena, .rule_collector = rule_collector} {}
    Impl(Impl const& other):arena(other.arena), rule_collector(other.rule_collector), solve_state{
      .arena = other.solve_state.arena,
      .rule_collector = other.solve_state.rule_collector,
      .conglomerate_class_info = copy_on_arena(other.arena, other.solve_state.conglomerate_class_info),
      .conglomerate_to_class = other.solve_state.conglomerate_to_class
    } {}
    ~Impl() {
      destroy_from_arena(arena, solve_state);
    }
    OwnedExpression reduce(OwnedExpression expr) {
      return reduce_generic(
        arena,
        std::move(expr),
        PipelineConfig{
          ReductionConfiguration{rule_collector},
          ArgumentReducerConfig{},
          ConglomerateReducerConfig{solve_state}
        },
        WeakKeyMemoizer{arena}
      );
    }
    OwnedExpression eliminate_conglomerates(WeakExpression expr) {
      return solve_state.eliminate_conglomerates(expr);
    }
    bool is_lambda_like(WeakExpression expr) {
      return NormalConglomerateReducer{
        arena, rule_collector, solve_state
      }.is_lambda_like(expr);
    }
    void request_assume_equal(OwnedExpression lhs, OwnedExpression rhs) {
      solve_state.create_conglomerate(mdb::make_vector(std::move(lhs), std::move(rhs)));
    }
    std::optional<EvaluationError> canonicalize_context() {
    RESTART_CANONICALIZATION:
      for(std::size_t class_index = 0; class_index < solve_state.conglomerate_class_info.size(); ++class_index) {
        auto& class_info = solve_state.conglomerate_class_info[class_index];
        for(std::size_t reducer_index = 0; reducer_index < class_info.reducers.active.size(); ++reducer_index) {
          auto& reducer = class_info.reducers.active[reducer_index];
          auto reduced = RepresentativeReducer{
            arena, rule_collector, solve_state, class_index, reducer_index
          }.reduce_outer(arena.copy(reducer));
          if(reducer != reduced) {
            arena.drop(std::move(reducer));
            reducer = std::move(reduced);
            bool okay = solve_state.examine_updated_reducer(class_index, reducer_index);
            if(!okay)
              return EvaluationError{};
            goto RESTART_CANONICALIZATION;
          } else {
            arena.drop(std::move(reduced));
          }
        }
        while(!class_info.reducers.waiting.empty()) {
          auto reducer = std::move(class_info.reducers.waiting.back());
          class_info.reducers.waiting.pop_back();
          auto reducer_index = class_info.reducers.active.size();
          class_info.reducers.active.push_back(arena.copy(reducer));
          auto reduced = RepresentativeReducer{
            arena, rule_collector, solve_state, class_index, reducer_index
          }.reduce_outer(std::move(reducer));
          arena.drop(std::move(class_info.reducers.active.back()));
          class_info.reducers.active.back() = std::move(reduced);
          bool okay = solve_state.examine_updated_reducer(class_index, reducer_index);
          if(!okay)
            return EvaluationError{};
          goto RESTART_CANONICALIZATION;
        }
      }
      return std::nullopt;
    }
    AssumptionInfo list_assumptions() {
      return solve_state.list_assumptions();
    }
    std::optional<PartialMap> try_to_map_to(EvaluationContext& target, MapRequest request) {
      return solve_state.try_to_map_to(target, std::move(request));
    }
  };
  EvaluationContext::EvaluationContext(Arena& arena, RuleCollector& rule_collector):impl(std::make_unique<Impl>(arena, rule_collector)) {}
  EvaluationContext::EvaluationContext(EvaluationContext const& other):impl(std::make_unique<Impl>(*other.impl)) {}
  EvaluationContext::EvaluationContext(EvaluationContext&&) = default;
  EvaluationContext& EvaluationContext::operator=(EvaluationContext const& other) {
    if(other.impl) {
      impl = std::make_unique<Impl>(*other.impl);
    } else {
      impl = nullptr;
    }
    return *this;
  }
  EvaluationContext& EvaluationContext::operator=(EvaluationContext&&) = default;
  EvaluationContext::~EvaluationContext() = default;

  OwnedExpression EvaluationContext::reduce(OwnedExpression expr) {
    return impl->reduce(std::move(expr));
  }
  OwnedExpression EvaluationContext::eliminate_conglomerates(OwnedExpression expr) {
    auto ret = impl->eliminate_conglomerates(expr);
    impl->arena.drop(std::move(expr));
    return ret;
  }
  bool EvaluationContext::is_lambda_like(WeakExpression expr) {
    return impl->is_lambda_like(expr);
  }
  std::optional<EvaluationError> EvaluationContext::assume_equal(OwnedExpression lhs, OwnedExpression rhs) {
    impl->request_assume_equal(std::move(lhs), std::move(rhs));
    return canonicalize_context();
  }
  std::optional<EvaluationError> EvaluationContext::canonicalize_context() {
    return impl->canonicalize_context();
  }
  AssumptionInfo EvaluationContext::list_assumptions() {
    return impl->list_assumptions();
  }
  std::optional<PartialMap> EvaluationContext::try_to_map_to(EvaluationContext& target, MapRequest request) {
    return impl->try_to_map_to(target, std::move(request));
  }
  bool PartialMap::can_map(Arena& arena, WeakExpression expr) {
    return arena.visit(expr, mdb::overloaded{
      [&](Apply const& apply) {
        return can_map(arena, apply.lhs) && can_map(arena, apply.rhs);
      },
      [&](Argument const& arg) {
        return arg_map.contains(arg.index);
      },
      [&](Conglomerate const& conglom) {
        return conglomerate_map.contains(conglom.index);
      },
      [&](Axiom const&) {
        return true;
      },
      [&](Declaration const&) {
        return true;
      },
      [&](Data const& data) {
        return arena.all_subexpressions_of(data, [&](WeakExpression expr) {
          return can_map(arena, expr);
        });
      }
    });
  }
  OwnedExpression PartialMap::map(Arena& arena, WeakExpression expr) {
    return arena.visit(expr, mdb::overloaded{
      [&](Apply const& apply) {
        return arena.apply(
          map(arena, apply.lhs),
          map(arena, apply.rhs)
        );
      },
      [&](Argument const& arg) {
        return arena.copy(arg_map.at(arg.index));
      },
      [&](Conglomerate const& conglom) {
        return arena.copy(conglomerate_map.at(conglom.index));
      },
      [&](Axiom const&) {
        return arena.copy(expr);
      },
      [&](Declaration const&) {
        return arena.copy(expr);
      },
      [&](Data const& data) -> OwnedExpression {
        return arena.modify_subexpressions_of(data, [&](WeakExpression expr) {
          return map(arena, expr);
        });
      }
    });
  }
}
