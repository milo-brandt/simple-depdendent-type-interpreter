#include "solver.hpp"
#include <unordered_map>
#include <optional>
#include <unordered_set>
#include "../Utility/vector_utility.hpp"
#include "../Utility/overloaded.hpp"
#include "../NewExpression/arena_utility.hpp"

namespace solver {
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Arena = new_expression::Arena;
  using Stack = stack::Stack;
  namespace {
    struct EquationInfo {
      Equation equation;
      bool handled = false;
      bool failed = false;
      static constexpr auto part_info = mdb::parts::simple<3>;
    };
    enum class AttemptResult {
      nothing,
      handled,
      failed
    };
    bool is_definitive(AttemptResult result) {
      return result != AttemptResult::nothing;
    }
    struct DefinitionFormData {
      WeakExpression head;
      std::size_t arg_count;
      std::unordered_map<std::uint64_t, OwnedExpression> arg_map;
      std::unordered_map<std::uint64_t, OwnedExpression> conglomerate_map;
      Stack target_stack;
      static constexpr auto part_info = mdb::parts::simple<5>;
    };
    std::optional<DefinitionFormData> is_term_in_definition_form(Arena& arena, WeakExpression term, Stack stack, mdb::function<std::optional<DefinableInfo>(new_expression::WeakExpression)>& get_definable_info) {
      auto unfolded = unfold(arena, term);
      if(auto definable_info = get_definable_info(unfolded.head)) {
        struct Detail {
          Arena& arena;
          Stack target_stack;
          std::unordered_map<std::uint64_t, OwnedExpression> arg_map;
          std::unordered_map<std::uint64_t, OwnedExpression> conglomerate_map;
          bool match_pieces(WeakExpression initial_part, WeakExpression target_part) {
            if(auto* arg = arena.get_if_argument(initial_part)) {
              if(arg_map.contains(arg->index)) {
                return arg_map.at(arg->index) == target_part;
              } else {
                arg_map.insert(std::make_pair(arg->index, arena.copy(target_part)));
                return true;
              }
            } else if(auto* conglom = arena.get_if_conglomerate(initial_part)) {
              if(conglomerate_map.contains(conglom->index)) {
                return conglomerate_map.at(conglom->index) == target_part;
              } else {
                conglomerate_map.insert(std::make_pair(conglom->index, arena.copy(target_part)));
                return true;
              }
            } else {
              auto initial_unfolded = unfold(arena, initial_part);
              auto target_unfolded = unfold(arena, target_part);
              if(initial_unfolded.head == target_unfolded.head && initial_unfolded.args.size() == target_unfolded.args.size() && arena.holds_axiom(initial_unfolded.head)) {
                for(std::size_t i = 0; i < initial_unfolded.args.size(); ++i) {
                  if(!match_pieces(initial_unfolded.args[i], target_unfolded.args[i])) return false;
                }
                return true;
              } else {
                return false;
              }
            }
          }
          bool match_to_arg(WeakExpression initial_arg, std::size_t target_arg) {
            OwnedExpression output = target_stack.reduce(arena.argument(target_arg));
            new_expression::RAIIDestroyer destroyer{arena, output};
            return match_pieces(initial_arg, output);
          }
        };
        Detail detail{
          .arena = arena,
          .target_stack = std::move(definable_info->stack)
        };
        for(std::size_t i = 0; i < unfolded.args.size(); ++i) {
          if(!detail.match_to_arg(unfolded.args[i], i)) {
            destroy_from_arena(arena, detail.arg_map, detail.conglomerate_map);
            return std::nullopt;
          }
        }
        return DefinitionFormData{
          .head = unfolded.head,
          .arg_count = unfolded.args.size(),
          .arg_map = std::move(detail.arg_map),
          .conglomerate_map = std::move(detail.conglomerate_map),
          .target_stack = std::move(detail.target_stack)
        };
      } else {
        return std::nullopt;
      }
    }
    OwnedExpression apply_args_enumerated(Arena& arena, OwnedExpression head, std::uint64_t arg_count) {
      auto ret = std::move(head);
      for(std::uint64_t i = 0; i < arg_count; ++i) {
        ret = arena.apply(std::move(ret), arena.argument(i));
      }
      return ret;
    }
    OwnedExpression apply_args_vector(Arena& arena, OwnedExpression head, std::vector<std::uint64_t> const& args) {
      auto ret = std::move(head);
      for(auto arg : args) {
        ret = arena.apply(std::move(ret), arena.argument(arg));
      }
      return ret;
    }
    /*
      Utilities for (I)
    */
    std::optional<OwnedExpression> get_replacement_from(DefinitionFormData& definition_form, WeakExpression term, SolverInterface& interface) {
      struct Detail {
        SolverInterface& interface;
        DefinitionFormData& definition_form;
        bool is_acceptable(WeakExpression expr) {
          return interface.arena.visit(expr, mdb::overloaded{
            [&](new_expression::Apply const& apply) {
              return is_acceptable(apply.lhs) && is_acceptable(apply.rhs);
            },
            [&](new_expression::Axiom const&) {
              return true;
            },
            [&](new_expression::Declaration const&) {
              return !interface.term_depends_on(expr, definition_form.head);
            },
            [&](new_expression::Argument const& arg) {
              return definition_form.arg_map.contains(arg.index);
            },
            [&](new_expression::Conglomerate const& conglomerate) {
              return definition_form.conglomerate_map.contains(conglomerate.index);
            },
            [&](auto const&) -> bool {
              std::terminate();
            }
          });
        }
        OwnedExpression remap(WeakExpression expr) {
          return interface.arena.visit(expr, mdb::overloaded{
            [&](new_expression::Apply const& apply) {
              return interface.arena.apply(
                remap(apply.lhs),
                remap(apply.rhs)
              );
            },
            [&](new_expression::Axiom const&) {
              return interface.arena.copy(expr);
            },
            [&](new_expression::Declaration const&) {
              return interface.arena.copy(expr);
            },
            [&](new_expression::Argument const& arg) {
              return interface.arena.copy(definition_form.arg_map.at(arg.index));
            },
            [&](new_expression::Conglomerate const& conglomerate) {
              return interface.arena.copy(definition_form.conglomerate_map.at(conglomerate.index));
            },
            [&](auto const&) -> OwnedExpression {
              std::terminate();
            }
          });
        }
      };
      Detail detail{interface, definition_form};
      if(detail.is_acceptable(term)) {
        return detail.remap(term);
      }
      else return std::nullopt;
    }
    struct ExtractedRule {
      WeakExpression head;
      std::uint64_t arg_count;
      OwnedExpression replacement;
      static constexpr auto part_info = mdb::parts::simple<3>;
    };
    std::optional<ExtractedRule> get_rule_from_equation_lhs(WeakExpression lhs, WeakExpression rhs, Stack stack, SolverInterface& interface) {
      if(auto map = is_term_in_definition_form(interface.arena, lhs, stack, interface.get_definable_info)) {
        if(auto replacement = get_replacement_from(*map, rhs, interface)) {
          destroy_from_arena(interface.arena, *map);
          return ExtractedRule{
            .head = map->head,
            .arg_count = map->arg_count,
            .replacement = std::move(*replacement)
          };
        } else {
          destroy_from_arena(interface.arena, *map);
        }
      }
      return std::nullopt;
    }
    std::optional<ExtractedRule> get_rule_from_equation(WeakExpression lhs, WeakExpression rhs, Stack stack, SolverInterface& interface) {
      if(auto ret = get_rule_from_equation_lhs(lhs, rhs, stack, interface)) return ret;
      else return get_rule_from_equation_lhs(rhs, lhs, stack, interface);
    }
    /*
      Utilities for asymmetric explosion
    */
    /*struct AsymmetricExplodeSpec {
      WeakExpression pattern_head;
      std::vector<std::uint64_t> pattern_args;
      std::variant<WeakExpression, new_expression::Argument> irreducible_head;
      std::vector<WeakExpression> irreducible_args;
    };
    struct AsymmetricHeadFailure{};
    std::variant<std::monostate, AsymmetricHeadFailure, AsymmetricExplodeSpec> get_asymmetric_explode_from_equation(WeakExpression lhs, WeakExpression rhs, SolverInterface& interface) {
      //first must check lhs is head-closed!
      if(auto def_form = is_term_in_definition_form(interface.arena, lhs, interface.is_definable_indeterminate)) {
        auto unfolded = unfold(interface.arena, rhs);
        AsymmetricExplodeSpec ret{
          .pattern_head = def_form->head,
          .pattern_args = std::move(def_form->arg_list),
          .irreducible_head = new_expression::Argument{(std::uint64_t)-1},
          .irreducible_args = std::move(unfolded.args)
        };
        if(auto* arg = interface.arena.get_if_argument(unfolded.head)) {
          if(def_form->arg_to_index.contains(arg->index)) {
            ret.irreducible_head = new_expression::Argument{def_form->arg_to_index.at(arg->index)};
          } else {
            return AsymmetricHeadFailure{};
          }
        } else {
          ret.irreducible_head = unfolded.head;
        }
        return ret;
      }
      return std::monostate{};
    }*/
  }
  struct Solver::Impl {
    SolverInterface interface;
    std::vector<EquationInfo> equations;
    AttemptResult try_to_extract_rule(std::uint64_t index, EquationInfo const& eq) {
      if(auto extracted_rule = get_rule_from_equation(eq.equation.lhs, eq.equation.rhs, eq.equation.stack, interface)) {
        interface.make_definition({
          .head = extracted_rule->head,
          .arg_count = extracted_rule->arg_count,
          .replacement = std::move(extracted_rule->replacement)
        });
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    AttemptResult try_to_deepen(std::uint64_t index, EquationInfo const& eq) {
      if(interface.is_lambda_like(eq.equation.lhs)) {
        /*auto lhs_type = context.expression_context().get_domain_and_codomain(
          info.equation.stack.type_of(context.expression_context(), lhs.expression)
        );
        if(!lhs_type) std::terminate();*/
        equations.push_back({
          .equation = {
            .lhs = interface.arena.apply(
              interface.arena.copy(eq.equation.lhs),
              interface.arena.argument(eq.equation.stack.depth())
            ),
            .rhs = interface.arena.apply(
              interface.arena.copy(eq.equation.rhs),
              interface.arena.argument(eq.equation.stack.depth())
            ),
            .stack = eq.equation.stack.extend(interface.arena.axiom()) //placeholder
          }
        });
        return AttemptResult::handled;
      } else if(interface.is_lambda_like(eq.equation.rhs)) {
        /*auto rhs_type = context.expression_context().get_domain_and_codomain(
          info.equation.stack.type_of(context.expression_context(), rhs.expression)
        );
        if(!rhs_type) std::terminate();*/
        equations.push_back({
          .equation = {
            .lhs = interface.arena.apply(
              interface.arena.copy(eq.equation.lhs),
              interface.arena.argument(eq.equation.stack.depth())
            ),
            .rhs = interface.arena.apply(
              interface.arena.copy(eq.equation.rhs),
              interface.arena.argument(eq.equation.stack.depth())
            ),
            .stack = eq.equation.stack.extend(interface.arena.axiom()) //placeholder
          }
        });
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    AttemptResult try_to_explode_symmetric(std::uint64_t index, EquationInfo const& eq) {
      if(interface.is_head_closed(eq.equation.lhs) && interface.is_head_closed(eq.equation.rhs)) {
        auto unfold_lhs = unfold(interface.arena, eq.equation.lhs);
        auto unfold_rhs = unfold(interface.arena, eq.equation.rhs);
        if(unfold_lhs.head == unfold_rhs.head && unfold_lhs.args.size() == unfold_rhs.args.size()) {
          auto stack = eq.equation.stack; //move this out of the way, in case pushing to the vector moves stuff
          for(std::uint64_t i = 0; i < unfold_lhs.args.size(); ++i) {
            equations.push_back({
              .equation = {
                .lhs = interface.arena.copy(unfold_lhs.args[i]),
                .rhs = interface.arena.copy(unfold_rhs.args[i]),
                .stack = stack
              }
            });
          }
          return AttemptResult::handled;
        } else {
          return AttemptResult::failed;
        }
      }
      return AttemptResult::nothing;
    }
    AttemptResult try_to_judge_equal(std::uint64_t index, EquationInfo const& eq) {
      if(eq.equation.lhs == eq.equation.rhs) {
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    bool examine_equation(std::uint64_t index) {
      auto& eq = equations[index];
      if(eq.handled || eq.failed) std::terminate(); //precondition
      eq.equation.lhs = eq.equation.stack.reduce(std::move(eq.equation.lhs));
      eq.equation.rhs = eq.equation.stack.reduce(std::move(eq.equation.rhs));

      AttemptResult ret = AttemptResult::nothing;
      std::apply([&](auto... tests) {
        (is_definitive(ret = (this->*tests)(index, eq)) || ...);
      }, std::make_tuple(
        &Impl::try_to_extract_rule,
        &Impl::try_to_deepen,
        &Impl::try_to_explode_symmetric,
        &Impl::try_to_judge_equal)
      );
      auto& eq_final = equations[index]; //Vector might move!!!
      if(ret == AttemptResult::handled) {
        eq_final.handled = true;
        return true;
      } else if(ret == AttemptResult::failed) {
        eq_final.failed = true;
        return true;
      } else {
        return false;
      }
    }
    bool solved() {
      for(auto& eq : equations) {
        if(eq.failed || !eq.handled) return false;
      }
      return true;
    }
    bool failed() {
      for(auto& eq : equations) {
        if(eq.failed) return true;
      }
      return false;
    }
    bool try_to_make_progress() {
      bool made_progress = false;
      std::uint64_t start_point = 0;
    DEDUCE_START:
      for(std::uint64_t i = start_point; i < equations.size(); ++i) {
        if(!equations[i].handled && !equations[i].failed) {
          if(examine_equation(i)) {
            made_progress = true;
            start_point = i + 1;
            goto DEDUCE_START;
          }
        }
      }
      for(std::uint64_t i = 0; i < start_point; ++i) {
        if(!equations[i].handled && !equations[i].failed) {
          if(examine_equation(i)) {
            made_progress = true;
            start_point = i + 1;
            goto DEDUCE_START;
          }
        }
      }
      return made_progress;
    }
    void close() {
      //nothing to do
    }
    EquationErrorInfo get_error_info() {
      EquationErrorInfo info {
        .primary = std::move(equations[0].equation)
      };
      info.primary_failed = equations[0].failed;
      bool first = true;
      for(auto& eq : equations) {
        if(first) {
          first = false;
          continue;
        }
        if(eq.failed) {
          info.failures.push_back(std::move(eq.equation));
        } else if(!eq.handled) {
          info.stalls.push_back(std::move(eq.equation));
        }
      }
      return info;
    }
    ~Impl() {
      destroy_from_arena(interface.arena, equations);
    }
  };
  Solver::Solver(SolverInterface interface, Equation equation):impl(new Impl{
    .interface = std::move(interface),
  }) {
    impl->equations.push_back({
      std::move(equation)
    });
  }
  Solver::Solver(Solver&&) = default;
  Solver& Solver::operator=(Solver&&) = default;
  Solver::~Solver() = default;
  bool Solver::try_to_make_progress() {
    return impl->try_to_make_progress();
  }
  void Solver::close() {
    return impl->close();
  }
  bool Solver::solved() {
    return impl->solved();
  }
  bool Solver::failed() {
    return impl->failed();
  }
  EquationErrorInfo Solver::get_error_info() && {
    return impl->get_error_info();
  }
}
