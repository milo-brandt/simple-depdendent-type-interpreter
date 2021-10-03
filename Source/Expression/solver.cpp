#include "solver.hpp"
#include <unordered_map>
#include <optional>
#include <unordered_set>
#include "../Utility/vector_utility.hpp"

namespace expression::solver {
  namespace {
    struct Listener {
      bool handled = false;
      std::uint64_t equations_remaining;
      std::uint64_t base_index;
      mdb::Promise<std::optional<SolveError> > promise;
    };
    struct EquationInfo {
      Equation equation;
      std::size_t indeterminate_context;
      std::uint64_t parent = std::uint64_t(-1); //-1 for no parent
      bool handled = false; //not necessarily satisfied - but no further action needed
      bool failed = false;
      std::shared_ptr<Listener> listener;
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
      std::uint64_t head;                                               //checked to be variable
      std::vector<std::uint64_t> arg_list;                              //argument at each index
      std::unordered_map<std::uint64_t, std::uint64_t> arg_to_index;    //which arguments appear at which indices
    };
    std::optional<DefinitionFormData> is_term_in_definition_form(tree::Expression const& term, std::unordered_set<std::uint64_t> const& variables) {
      auto unfolded = unfold(term);
      if(auto const* ext = unfolded.head.get_if_external()) {
        if(variables.contains(ext->external_index)) {
          DefinitionFormData ret{
            .head = ext->external_index
          };
          for(std::uint64_t index = 0; index < unfolded.args.size(); ++index) {
            if(auto const* arg_data = unfolded.args[index].get_if_arg()) {
              if(ret.arg_to_index.contains(arg_data->arg_index)) {
                return std::nullopt; //a single argument is applied twice
              } else {
                ret.arg_list.push_back(arg_data->arg_index);
                ret.arg_to_index.insert(std::make_pair(arg_data->arg_index, index));
              }
            } else {
              return std::nullopt; //applied value is not argument
            }
          }
          return std::move(ret);
        }
      }
      return std::nullopt;
    }
    tree::Expression apply_args_enumerated(tree::Expression head, std::uint64_t arg_count) {
      tree::Expression ret = std::move(head);
      for(std::uint64_t i = 0; i < arg_count; ++i) {
        ret = tree::Apply{std::move(ret), tree::Arg{i}};
      }
      return ret;
    }
    tree::Expression apply_args_vector(tree::Expression head, std::vector<std::uint64_t> const& args) {
      tree::Expression ret = std::move(head);
      for(auto arg : args) {
        ret = tree::Apply{std::move(ret), tree::Arg{arg}};
      }
      return ret;
    }
    std::variant<tree::External, tree::Arg> get_head_variant(tree::Expression term) {
      if(auto* arg = term.get_if_arg()) {
        return std::move(*arg);
      } else {
        return std::move(term.get_external());
      }
    }
    /*
      Utilities for (I)
    */
    std::optional<tree::Expression> get_replacement_from(std::unordered_map<std::uint64_t, std::uint64_t> const& map, tree::Expression const& term, Context& context, std::uint64_t head) {
      struct Detail {
        Context& context;
        std::unordered_map<std::uint64_t, std::uint64_t> const& arg_map;
        std::uint64_t head;
        bool is_acceptable(tree::Expression const& expr) {
          return expr.visit(mdb::overloaded{
            [&](tree::Apply const& apply) {
              return is_acceptable(apply.lhs) && is_acceptable(apply.rhs);
            },
            [&](tree::External const& external) {
              return !context.term_depends_on(external.external_index, head);
            },
            [&](tree::Arg const& arg) {
              return arg_map.contains(arg.arg_index);
            },
            [&](tree::Data const& data) {
              bool okay = true;
              data.data.visit_children([&](tree::Expression const& expr) {
                okay = okay && is_acceptable(expr);
              });
              return okay;
            }
          });
        }
      };
      Detail detail{context, map, head};
      if(detail.is_acceptable(term)) return remap_args(map, term);
      else return std::nullopt;
    }
    struct ExtractedRule {
      std::uint64_t head;
      std::uint64_t arg_count;
      tree::Expression replacement;
    };
    std::optional<ExtractedRule> get_rule_from_equation_lhs(tree::Expression const& lhs, tree::Expression const& rhs, std::unordered_set<std::uint64_t> const& variables, Context& context) {
      if(auto map = is_term_in_definition_form(lhs, variables)) {
        if(auto tree = get_replacement_from(map->arg_to_index, rhs, context, map->head)) {
          return ExtractedRule{
            .head = map->head,
            .arg_count = map->arg_to_index.size(),
            .replacement = std::move(*tree)
          };
        }
      }
      return std::nullopt;
    }
    std::optional<ExtractedRule> get_rule_from_equation(tree::Expression const& lhs, tree::Expression const& rhs, std::unordered_set<std::uint64_t> const& variables, Context& context) {
      if(auto ret = get_rule_from_equation_lhs(lhs, rhs, variables, context)) return ret;
      else return get_rule_from_equation_lhs(rhs, lhs, variables, context);
    }
    /*
      Utilities for asymmetric explosion
    */
    struct AsymmetricExplodeSpec {
      std::uint64_t pattern_head;
      std::vector<std::uint64_t> pattern_args;
      std::variant<tree::External, tree::Arg> irreducible_head;
      std::vector<tree::Expression> irreducible_args;
    };
    struct AsymmetricHeadFailure{};
    std::variant<std::monostate, AsymmetricHeadFailure, AsymmetricExplodeSpec> get_asymmetric_explode_from_equation(tree::Expression const& lhs, tree::Expression const& rhs, std::unordered_set<std::uint64_t> const& variables) {
      //first must check lhs is head-closed!
      if(auto def_form = is_term_in_definition_form(lhs, variables)) {
        auto unfolded = unfold(rhs);
        AsymmetricExplodeSpec ret{
          .pattern_head = def_form->head,
          .pattern_args = std::move(def_form->arg_list),
          .irreducible_args = std::move(unfolded.args)
        };
        if(auto* arg = unfolded.head.get_if_arg()) {
          if(def_form->arg_to_index.contains(arg->arg_index)) {
            ret.irreducible_head = tree::Arg{def_form->arg_to_index.at(arg->arg_index)};
          } else {
            return AsymmetricHeadFailure{};
          }
        } else {
          ret.irreducible_head = unfolded.head.get_external();
        }
        return ret;
      }
      return std::monostate{};
    }
  }
  struct Solver::Impl {
    Context context;
    std::vector<std::unordered_set<std::uint64_t> > indeterminate_contexts;
    std::vector<EquationInfo> equations;
    std::vector<std::pair<std::uint64_t, mdb::Promise<std::optional<SolveError> > > > waiting_routines;
    AttemptResult try_to_extract_rule(std::uint64_t index, EquationInfo const& info, Simplification const& lhs, Simplification const& rhs) {
      if(auto extracted_rule = get_rule_from_equation(lhs.expression, rhs.expression, indeterminate_contexts[info.indeterminate_context], context)) {
        context.define_variable(extracted_rule->head, extracted_rule->arg_count, std::move(extracted_rule->replacement));
        indeterminate_contexts[info.indeterminate_context].erase(extracted_rule->head);
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    AttemptResult try_to_deepen(std::uint64_t index, EquationInfo const& info, Simplification const& lhs, Simplification const& rhs) {
      if(lhs.state == SimplificationState::lambda_like) {
        auto lhs_type = context.expression_context().get_domain_and_codomain(
          info.equation.stack.type_of(context.expression_context(), lhs.expression)
        );
        if(!lhs_type) std::terminate();
        equations.push_back({
          .equation = {
            .stack = info.equation.stack.extend(context.expression_context(), std::move(lhs_type->domain)),
            .lhs =  tree::Apply{lhs.expression, tree::Arg{info.equation.stack.depth()}},
            .rhs = tree::Apply{rhs.expression, tree::Arg{info.equation.stack.depth()}}
          },
          .indeterminate_context = info.indeterminate_context,
          .parent = index,
          .listener = info.listener
        });
        ++info.listener->equations_remaining;
        return AttemptResult::handled;
      } else if(rhs.state == SimplificationState::lambda_like) {
        auto rhs_type = context.expression_context().get_domain_and_codomain(
          info.equation.stack.type_of(context.expression_context(), rhs.expression)
        );
        if(!rhs_type) std::terminate();
        equations.push_back({
          .equation = {
            .stack = info.equation.stack.extend(context.expression_context(), std::move(rhs_type->domain)),
            .lhs =  tree::Apply{lhs.expression, tree::Arg{info.equation.stack.depth()}},
            .rhs = tree::Apply{rhs.expression, tree::Arg{info.equation.stack.depth()}}
          },
          .indeterminate_context = info.indeterminate_context,
          .parent = index,
          .listener = info.listener
        });
        ++info.listener->equations_remaining;
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    AttemptResult try_to_explode_symmetric(std::uint64_t index, EquationInfo const& info, Simplification const& lhs, Simplification const& rhs) {
      if(lhs.state == SimplificationState::head_closed && rhs.state == SimplificationState::head_closed) {
        auto unfold_lhs = unfold(lhs.expression);
        auto unfold_rhs = unfold(rhs.expression);
        if(unfold_lhs.head == unfold_rhs.head && unfold_lhs.args.size() == unfold_rhs.args.size()) {
          for(std::uint64_t i = 0; i < unfold_lhs.args.size(); ++i) {
            equations.push_back({
              .equation = {
                .stack = info.equation.stack,
                .lhs = unfold_lhs.args[i],
                .rhs = unfold_rhs.args[i],
              },
              .indeterminate_context = info.indeterminate_context,
              .parent = index,
              .listener = info.listener
            });
          }
          info.listener->equations_remaining += unfold_lhs.args.size();
          return AttemptResult::handled;
        } else {
          return AttemptResult::failed;
        }
      }
      return AttemptResult::nothing;
    }
    void process_asymmetric_explosion(std::uint64_t index, EquationInfo const& info, AsymmetricExplodeSpec spec) {
      auto replacement = std::visit([](auto const& x) -> tree::Expression { return x; }, spec.irreducible_head);
      for(std::uint64_t i = 0; i < spec.irreducible_args.size(); ++i) {
        auto next_type_opt = context.expression_context().get_domain_and_codomain(
          info.equation.stack.type_of(context.expression_context(), replacement)
        );
        if(!next_type_opt) std::terminate();
        auto var = context.introduce_variable(info.equation.stack.instance_of_type_family(context.expression_context(),
         std::move(next_type_opt->domain)
       ));
        indeterminate_contexts[info.indeterminate_context].insert(var);
        replacement = tree::Apply{std::move(replacement), apply_args_enumerated(tree::External{var}, spec.pattern_args.size())};
        equations.push_back({
          .equation = {
            .stack = info.equation.stack,
            .lhs = apply_args_vector(tree::External{var}, spec.pattern_args),
            .rhs = spec.irreducible_args[i],
          },
          .indeterminate_context = info.indeterminate_context,
          .parent = index,
          .listener = info.listener
        });
      }
      info.listener->equations_remaining += spec.irreducible_args.size();

      context.define_variable(
        spec.pattern_head,
        spec.pattern_args.size(),
        std::move(replacement)
      );
      indeterminate_contexts[info.indeterminate_context].erase(spec.pattern_head);
    }
    AttemptResult try_to_explode_asymmetric(std::uint64_t index, EquationInfo const& info, Simplification const& lhs, Simplification const& rhs) {
      if(rhs.state == SimplificationState::head_closed) {
        auto asymmetric = get_asymmetric_explode_from_equation(lhs.expression, rhs.expression, indeterminate_contexts[info.indeterminate_context]);
        if(std::holds_alternative<AsymmetricHeadFailure>(asymmetric)) return AttemptResult::failed;
        if(auto* success = std::get_if<AsymmetricExplodeSpec>(&asymmetric)) {
          process_asymmetric_explosion(index, info, std::move(*success));
          return AttemptResult::handled;
        }
      }
      if(lhs.state == SimplificationState::head_closed) {
        auto asymmetric = get_asymmetric_explode_from_equation(rhs.expression, lhs.expression, indeterminate_contexts[info.indeterminate_context]);
        if(std::holds_alternative<AsymmetricHeadFailure>(asymmetric)) return AttemptResult::failed;
        if(auto* success = std::get_if<AsymmetricExplodeSpec>(&asymmetric)) {
          process_asymmetric_explosion(index, info, std::move(*success));
          return AttemptResult::handled;
        }
      }
      return AttemptResult::nothing;
    }
    AttemptResult try_to_judge_equal(std::uint64_t index, EquationInfo const& stack, Simplification const& lhs, Simplification const& rhs) {
      if(lhs.expression == rhs.expression) {
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    bool examine_equation(std::uint64_t index) {
      auto info = std::move(equations[index]); //move the equation *out* of the stack - so it can't move
      if(info.handled || info.failed) std::terminate(); //precondition
      auto lhs = context.simplify(std::move(info.equation.lhs));
      auto rhs = context.simplify(std::move(info.equation.rhs));

      AttemptResult ret = AttemptResult::nothing;
      std::apply([&](auto... tests) {
        (is_definitive(ret = (this->*tests)(index, info, lhs, rhs)) || ...);
      }, std::make_tuple(
        &Impl::try_to_extract_rule,
        &Impl::try_to_deepen,
        &Impl::try_to_explode_symmetric,
        &Impl::try_to_explode_asymmetric,
        &Impl::try_to_judge_equal)
      );
      auto& info_final = equations[index]; //Vector might move!!!
      info_final = std::move(info); //move it back
      info_final.equation.lhs = std::move(lhs.expression);
      info_final.equation.rhs = std::move(rhs.expression);
      if(ret == AttemptResult::handled) {
        info_final.handled = true;
        if(--info_final.listener->equations_remaining == 0) {
          info_final.listener->handled = true;
          info_final.listener->promise.set_value(std::nullopt); //solved!
        }
        return true;
      } else if(ret == AttemptResult::failed) {
        info_final.failed = true;
        if(!info_final.listener->handled) {
          info_final.listener->handled = true;
          info_final.listener->promise.set_value(SolveError{
            .equation_id = info_final.listener->base_index,
            .failed = true
          });
        }
        return true;
      } else {
        return false;
      }
    }
    bool is_equation_satisfied(std::uint64_t index) {
      std::unordered_set<std::uint64_t> requirements;
      auto check_required_equation = [&](std::uint64_t i) {
        if(equations[i].handled) {
          requirements.insert(i);
          return true;
        } else {
          return false;
        }
      };
      if(!check_required_equation(index)) return false;
      for(std::uint64_t i = index + 1; i < equations.size(); ++i) {
        if(requirements.contains(equations[i].parent)) {
          if(!check_required_equation(i)) return false;
        }
      }
      return true;
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
    IndeterminateContext create_context(request::CreateContext create_context) {
      IndeterminateContext ret{
        .index = indeterminate_contexts.size()
      };
      indeterminate_contexts.push_back(std::move(create_context.indeterminates));
      return ret;
    }
    void register_indeterminate(request::RegisterIndeterminate register_indeterminate) {
      indeterminate_contexts[register_indeterminate.indeterminate_context.index].insert(register_indeterminate.new_variable);
    }
    mdb::Future<std::optional<SolveError> > solve(request::Solve solve) {
      auto eq_index = equations.size();
      auto [promise, future] = mdb::create_promise_future_pair<std::optional<SolveError> >();
      auto listener = std::shared_ptr<Listener>{new Listener{
        .equations_remaining = 1,
        .base_index = eq_index,
        .promise = std::move(promise)
      }};
      equations.push_back({
        .equation = std::move(solve.equation),
        .indeterminate_context = solve.indeterminate_context.index,
        .listener = std::move(listener)
      });
      waiting_routines.emplace_back(eq_index, std::move(promise));
      return std::move(future);
    }
    bool is_fully_satisfied() {
      for(auto const& equation : equations) {
        if(!equation.handled) return false;
      }
      return true;
    }
    void close() {
      for(auto const& equation : equations) {
        if(!equation.listener->handled) {
          equation.listener->promise.set_value(SolveError{
            .equation_id = equation.listener->base_index,
            .failed = false
          });
          equation.listener->handled = true;
        }
      }
    }
    SolveErrorInfo get_error_info(std::uint64_t index) {
      std::unordered_set<std::uint64_t> requirements;
      SolveErrorInfo ret{
        .primary = equations[index].equation
      };
      auto check_required_equation = [&](std::uint64_t i) {
        if(i != index) {
          if(equations[i].failed) {
            ret.secondary_fail.push_back(equations[i].equation);
          } else if(!equations[i].handled) {
            ret.secondary_stuck.push_back(equations[i].equation);
          }
        }
        requirements.insert(i);
      };
      check_required_equation(index);
      for(std::uint64_t i = index + 1; i < equations.size(); ++i) {
        if(requirements.contains(equations[i].parent)) {
          check_required_equation(i);
        }
      }
      return ret;
    }
  };
  Solver::Solver(Context context):impl(new Impl{.context = std::move(context)}) {
    impl->indeterminate_contexts.emplace_back();
  }
  IndeterminateContext Solver::create_context(request::CreateContext create_context) {
    return impl->create_context(std::move(create_context));
  }
  void Solver::register_indeterminate(request::RegisterIndeterminate register_indeterminate) {
    return impl->register_indeterminate(std::move(register_indeterminate));
  }
  mdb::Future<std::optional<SolveError> > Solver::solve(request::Solve solve) {
    return impl->solve(std::move(solve));
  }
  bool Solver::try_to_make_progress() {
    return impl->try_to_make_progress();
  }
  void Solver::close() {
    return impl->close();
  }
  SolveErrorInfo Solver::get_error_info(std::uint64_t equation_id) {
    return impl->get_error_info(equation_id);
  }

  Solver::Solver(Solver&&) = default;
  Solver& Solver::operator=(Solver&&) = default;
  Solver::~Solver() = default;

}
