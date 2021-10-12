#include "evaluation.hpp"
#include "arena_utility.hpp"
#include <algorithm>
#include "../Utility/vector_utility.hpp"
#include "../Utility/overloaded.hpp"

namespace new_expression {
  namespace {
    template<class Base> //Base must provide Arena& arena and OwnedExpression reduce()
    struct ReducerCRTP {
      Base& me() { return *(Base*)this; }
      OwnedExpression substitute_into_replacement(Arena& arena, WeakExpression substitution, std::span<WeakExpression> args) {
        return substitute_into(arena, substitution, args);
      }
      bool reduce_by_pattern(OwnedExpression& expr, RuleCollector const& collector) {
        auto& arena = me().arena;
        auto unfolded = unfold(arena, expr);
        if(arena.holds_declaration(unfolded.head)) {
          auto const& declaration_info = collector.declaration_info(unfolded.head);
          for(auto const& rule : declaration_info.rules) {
            if(rule.pattern_body.args_captured <= unfolded.args.size()) {
              std::vector<WeakExpression> pattern_stack;
              pattern_stack.insert(pattern_stack.end(), unfolded.args.begin(), unfolded.args.end());
              std::vector<OwnedExpression> novel_roots; //storage for new expressions we create
              for(auto const& match : rule.pattern_body.sub_matches) {
                auto new_expr = me().substitute_into_replacement(arena, match.substitution, mdb::as_span(pattern_stack));
                new_expr = me().reduce(std::move(new_expr));
                auto match_unfold = unfold(arena, new_expr);
                novel_roots.push_back(std::move(new_expr)); //keep reference for later deletion
                if(match_unfold.head != match.expected_head) goto PATTERN_FAILED;
                if(match_unfold.args.size() != match.args_captured) goto PATTERN_FAILED;
                pattern_stack.insert(pattern_stack.end(), match_unfold.args.begin(), match_unfold.args.end());
              }
              for(auto const& data_check : rule.pattern_body.data_checks) {
                auto new_expr = arena.copy(pattern_stack[data_check.capture_index]);
                new_expr = me().reduce(std::move(new_expr));
                pattern_stack[data_check.capture_index] = new_expr;
                bool success = [&] {
                  if(auto* data = arena.get_if_data(new_expr)) {
                    return data->type_index == data_check.expected_type;
                  } else {
                    return false;
                  }
                }();
                novel_roots.push_back(std::move(new_expr));
                if(!success) goto PATTERN_FAILED;
              }
              //if we get here, the pattern succeeded.
              {
                auto new_expr = me().substitute_into_replacement(arena, rule.replacement, mdb::as_span(pattern_stack));
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
    };
    struct SimpleReducer : ReducerCRTP<SimpleReducer> {
      Arena& arena;
      RuleCollector const& rule_collector;
      WeakKeyMap<OwnedExpression, PartDestroyer> reductions;
      OwnedExpression reduce(OwnedExpression expr) {
        if(reductions.contains(expr)) {
          auto ret = arena.copy(reductions.at(expr));
          arena.drop(std::move(expr));
          return ret;
        }
        WeakExpression input = expr;
        while(reduce_by_pattern(expr, rule_collector));
        reductions.set(input, arena.copy(expr));
        return std::move(expr);
      }
    };
  }
  SimpleEvaluationContext::SimpleEvaluationContext(Arena& arena, RuleCollector& rule_collector):arena(&arena), rule_collector(&rule_collector) {}
  OwnedExpression SimpleEvaluationContext::reduce_head(OwnedExpression expr) {
    return SimpleReducer{
      .arena = *arena,
      .rule_collector = *rule_collector,
      .reductions{*arena}
    }.reduce(std::move(expr));
  }
  namespace {
    namespace conglomerate_status {
      struct PureOpen {
        WeakExpression representative; //has to be equal to one of the reducers
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
      std::vector<ConglomerateInfo> conglomerate_class_info;
      std::vector<std::size_t> conglomerate_to_class;
      static constexpr auto part_info = mdb::parts::simple<3>;
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
          void combine(std::size_t target, std::size_t source) {
            if(target == source) return;
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
                if(source_axiomatic->head != target_axiomatic->head) std::terminate();
                if(source_axiomatic->applied_conglomerates.size() != target_axiomatic->applied_conglomerates.size()) std::terminate();
                me.arena.drop(std::move(target_axiomatic->head)); //target will be destroyed
                for(std::size_t i = 0; i < source_axiomatic->applied_conglomerates.size(); ++i) {
                  further_combines.emplace_back(
                    source_axiomatic->applied_conglomerates[i],
                    target_axiomatic->applied_conglomerates[i]
                  );
                }
              } else {
                target_class.status = std::move(*source_axiomatic);
              }
            } else if(auto* target_pure_open = std::get_if<conglomerate_status::PureOpen>(&target_class.status)) {
              //merge application_conglomerates
              auto& source_pure_open = std::get<conglomerate_status::PureOpen>(source_class.status);
              target_pure_open->application_conglomerates.insert(
                target_pure_open->application_conglomerates.end(),
                source_pure_open.application_conglomerates.begin(),
                source_pure_open.application_conglomerates.end()
              );
              std::sort(target_pure_open->application_conglomerates.begin(), target_pure_open->application_conglomerates.end());
              auto unique_end = std::unique(target_pure_open->application_conglomerates.begin(), target_pure_open->application_conglomerates.end());
              target_pure_open->application_conglomerates.erase(unique_end, target_pure_open->application_conglomerates.end());
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
          }
        };
        Detail detail{*this};
        detail.further_combines.emplace_back(source, target);
        while(!detail.further_combines.empty()) {
          auto [top_source, top_target] = detail.further_combines.back();
          detail.further_combines.pop_back();
          detail.combine(top_source, top_target);
        }
        return !exists_axiomatic_cycle(); //succeeds so long as no cycle is created.
      }
      void rotate_classes(std::size_t original_index, std::size_t new_index) { //must have new_index > original_index. Rotates new -> original, sending original -> original + 1
        std::rotate(
          conglomerate_class_info.begin() + original_index,
          conglomerate_class_info.begin() + new_index,
          conglomerate_class_info.begin() + new_index + 1
        );
        update_class_indices([&](std::size_t index) {
          if(index < original_index) {
            return index;
          } else if(index < new_index) {
            return index + 1;
          } else if(index == new_index){
            return original_index;
          } else {
            return index;
          }
        });
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
            .representative = std::move(representative_weak),
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
        conglomerate_class_info[target_class].status = conglomerate_status::Axiomatic{
          .head = std::move(head),
          .applied_conglomerates = std::move(applied_conglomerates),
          .conglomerate_index = std::get<0>(conglomerate_class_info[target_class].status).conglomerate_index
        };
      }
      bool has_computation(WeakExpression expr) { //if *any* pattern could possibly reduce expr.
        return false;
      }
      void drop_reducer(std::size_t target_class, std::size_t target_reducer) {
        arena.drop(std::move(conglomerate_class_info[target_class].reducers.active[target_reducer]));
        conglomerate_class_info[target_class].reducers.active.erase(conglomerate_class_info[target_class].reducers.active.begin() + target_reducer);
      }
      bool examine_updated_reducer(std::size_t target_class, std::size_t target_reducer) { //must be in "active" row - but presumably ignored in reduction. must also be reduced
        WeakExpression reducer = conglomerate_class_info[target_class].reducers.active[target_reducer];
        if(has_computation(reducer)) {
          return false; //no good - can't deal with expressions with computations
        }
        if(auto* conglomerate = arena.get_if_conglomerate(reducer)) {
          drop_reducer(target_class, target_reducer);
          return combine_conglomerates(target_class, conglomerate_to_class[conglomerate->index]);
        }
        auto unfolding = unfold(arena, reducer);
        if(arena.holds_axiom(unfolding.head)) {
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
        } else {
          return true; //okay - still purely open
        }
      }
      std::size_t conglomerate_index_of_class(std::size_t class_index) {
        return std::visit([](auto const& status) {
          return status.conglomerate_index;
        }, conglomerate_class_info[conglomerate_to_class[class_index]].status);
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
      //one slight issue: would like to make sure that if we have #0 -> #2, but #0 is an axiomatic
      //conglomerate, would rather merge 0 and 2 instead of seeing #0 = axiom #3 #4 and exploding #2
      ConglomerateSolveState& solve_state;
      std::size_t target_class;
      std::size_t target_index;
      bool is_representative;
      RepresentativeReducer(Arena& arena, RuleCollector const& rule_collector, ConglomerateSolveState& solve_state,
                            std::size_t target_class, std::size_t target_index, bool is_representative):
                            ConglomerateReducerCRTP<RepresentativeReducer>(arena, rule_collector),
                            solve_state(solve_state),
                            target_class(target_class),
                            target_index(target_index),
                            is_representative(is_representative){}
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
              if(is_representative && class_index >= target_class) {
                if(auto* pure_open = std::get_if<conglomerate_status::PureOpen>(&class_info.status)) {
                  pure_open->representative = reducer;
                  if(class_index == target_class) {
                    is_representative = false; //someone else just became the representative
                  } else {
                    solve_state.rotate_classes(class_index, target_class);
                    ++target_class;
                  }
                } else {
                  //not sure what happens here
                }
              }
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
    struct NormalConglomerateReducer : ConglomerateReducerCRTP<NormalConglomerateReducer> {
      ConglomerateSolveState& solve_state;
      NormalConglomerateReducer(Arena& arena, RuleCollector const& rule_collector, ConglomerateSolveState& solve_state):
        ConglomerateReducerCRTP<NormalConglomerateReducer>(arena, rule_collector),
        solve_state(solve_state){}
      std::optional<OwnedExpression> get_conglomerate_reduction(WeakExpression expr, bool) {
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
  }
  struct EvaluationContext::Impl {
    Arena& arena;
    RuleCollector const& rule_collector;
    ConglomerateSolveState solve_state;
    Impl(Arena& arena, RuleCollector const& rule_collector):arena(arena), rule_collector(rule_collector), solve_state{.arena = arena} {}
    ~Impl() {
      destroy_from_arena(arena, solve_state);
    }
    OwnedExpression reduce(OwnedExpression expr) {
      return NormalConglomerateReducer{
        arena, rule_collector, solve_state
      }.reduce_outer(std::move(expr));
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
          bool is_representative = [&] {
            if(auto* pure_open = std::get_if<conglomerate_status::PureOpen>(&class_info.status)) {
              return pure_open->representative == reducer;
            } else {
              return false;
            }
          }();
          auto reduced = RepresentativeReducer{
            arena, rule_collector, solve_state, class_index, reducer_index, is_representative
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
            arena, rule_collector, solve_state, class_index, reducer_index, false
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
  };
  EvaluationContext::EvaluationContext(Arena& arena, RuleCollector& rule_collector):impl(std::make_unique<Impl>(arena, rule_collector)) {}
  EvaluationContext::EvaluationContext(EvaluationContext&&) = default;
  EvaluationContext& EvaluationContext::operator=(EvaluationContext&&) = default;
  EvaluationContext::~EvaluationContext() = default;

OwnedExpression EvaluationContext::reduce(OwnedExpression expr) {
    return impl->reduce(std::move(expr));
  }
  std::optional<EvaluationError> EvaluationContext::assume_equal(OwnedExpression lhs, OwnedExpression rhs) {
    impl->request_assume_equal(std::move(lhs), std::move(rhs));
    return canonicalize_context();
  }
  std::optional<EvaluationError> EvaluationContext::canonicalize_context() {
    return impl->canonicalize_context();
  }
}