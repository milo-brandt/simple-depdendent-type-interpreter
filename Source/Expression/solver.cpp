#include "solver.hpp"
#include <unordered_map>
#include <optional>
#include <unordered_set>

namespace expression::solver {
  namespace {
    struct Equation {
      expression::Stack stack;
      tree::Expression lhs;
      tree::Expression rhs;
      std::uint64_t parent = std::uint64_t(-1); //-1 for no parent
      bool handled = false; //not necessarily satisfied - but no further action needed
      bool failed = false;
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
      auto unfolded = unfold_ref(term);
      if(auto const* ext = unfolded.head->get_if_external()) {
        if(variables.contains(ext->external_index)) {
          DefinitionFormData ret{
            .head = ext->external_index
          };
          for(std::uint64_t index = 0; index < unfolded.args.size(); ++index) {
            if(auto const* arg_data = unfolded.args[index]->get_if_arg()) {
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
      return term.visit(mdb::overloaded{
        [&](tree::Apply const& apply) -> std::optional<tree::Expression> {
          if(auto lhs = get_replacement_from(map, apply.lhs, context, head)) {
            if(auto rhs = get_replacement_from(map, apply.rhs, context, head)) {
              return tree::Apply{std::move(*lhs), std::move(*rhs)};
            }
          }
          return std::nullopt;
        },
        [&](tree::Arg const& arg) -> std::optional<tree::Expression> {
          if(map.contains(arg.arg_index)) {
            return tree::Arg{map.at(arg.arg_index)};
          } else {
            return std::nullopt;
          }
        },
        [&](tree::External const& ext) -> std::optional<tree::Expression> {
          if(context.term_depends_on(head, ext.external_index)) {
            return std::nullopt;
          } else {
            return tree::Expression{ext};
          }
        }
      });
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
    std::vector<Equation> equations;
    std::unordered_set<std::uint64_t> variables;
    AttemptResult try_to_extract_rule(std::uint64_t index, expression::Stack stack, Simplification const& lhs, Simplification const& rhs) {
      if(auto extracted_rule = get_rule_from_equation(lhs.expression, rhs.expression, variables, context)) {
        context.define_variable(extracted_rule->head, extracted_rule->arg_count, std::move(extracted_rule->replacement));
        variables.erase(extracted_rule->head);
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    AttemptResult try_to_deepen(std::uint64_t index, expression::Stack stack, Simplification const& lhs, Simplification const& rhs) {
      if(lhs.state == SimplificationState::lambda_like) {
        auto lhs_type = context.expression_context().get_domain_and_codomain(
          stack.type_of(context.expression_context(), lhs.expression)
        );
        if(!lhs_type) std::terminate();
        equations.push_back({
          .stack = stack.extend(context.expression_context(), std::move(lhs_type->domain)),
          .lhs =  tree::Apply{lhs.expression, tree::Arg{stack.depth()}},
          .rhs = tree::Apply{rhs.expression, tree::Arg{stack.depth()}},
          .parent = index
        });
        return AttemptResult::handled;
      } else if(rhs.state == SimplificationState::lambda_like) {
        auto rhs_type = context.expression_context().get_domain_and_codomain(
          stack.type_of(context.expression_context(), rhs.expression)
        );
        if(!rhs_type) std::terminate();
        equations.push_back({
          .stack = stack.extend(context.expression_context(), std::move(rhs_type->domain)),
          .lhs =  tree::Apply{lhs.expression, tree::Arg{stack.depth()}},
          .rhs = tree::Apply{rhs.expression, tree::Arg{stack.depth()}},
          .parent = index
        });
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    AttemptResult try_to_explode_symmetric(std::uint64_t index, expression::Stack stack, Simplification const& lhs, Simplification const& rhs) {
      if(lhs.state == SimplificationState::head_closed && rhs.state == SimplificationState::head_closed) {
        auto unfold_lhs = unfold(lhs.expression);
        auto unfold_rhs = unfold(rhs.expression);
        if(unfold_lhs.head == unfold_rhs.head && unfold_lhs.args.size() == unfold_rhs.args.size()) {
          for(std::uint64_t i = 0; i < unfold_lhs.args.size(); ++i) {
            equations.push_back({
              .stack = stack,
              .lhs = unfold_lhs.args[i],
              .rhs = unfold_rhs.args[i],
              .parent = index
            });
          }
          return AttemptResult::handled;
        } else {
          return AttemptResult::failed;
        }
      }
      return AttemptResult::nothing;
    }
    void process_asymmetric_explosion(std::uint64_t index, expression::Stack stack, AsymmetricExplodeSpec spec) {
      auto replacement = std::visit([](auto const& x) -> tree::Expression { return x; }, spec.irreducible_head);
      for(std::uint64_t i = 0; i < spec.irreducible_args.size(); ++i) {
        auto next_type_opt = context.expression_context().get_domain_and_codomain(
          stack.type_of(context.expression_context(), replacement)
        );
        if(!next_type_opt) std::terminate();
        auto var = context.introduce_variable(std::move(next_type_opt->domain));
        variables.insert(var);
        replacement = tree::Apply{std::move(replacement), apply_args_enumerated(tree::External{var}, spec.pattern_args.size())};
        equations.push_back({
          .stack = stack,
          .lhs = apply_args_vector(tree::External{var}, spec.pattern_args),
          .rhs = spec.irreducible_args[i],
          .parent = index
        });
      }

      variables.erase(spec.pattern_head);
      context.define_variable(
        spec.pattern_head,
        spec.pattern_args.size(),
        std::move(replacement)
      );
      variables.erase(spec.pattern_head);
    }
    AttemptResult try_to_explode_asymmetric(std::uint64_t index, expression::Stack stack, Simplification const& lhs, Simplification const& rhs) {
      if(rhs.state == SimplificationState::head_closed) {
        auto asymmetric = get_asymmetric_explode_from_equation(lhs.expression, rhs.expression, variables);
        if(std::holds_alternative<AsymmetricHeadFailure>(asymmetric)) return AttemptResult::failed;
        if(auto* success = std::get_if<AsymmetricExplodeSpec>(&asymmetric)) {
          process_asymmetric_explosion(index, stack, std::move(*success));
          return AttemptResult::handled;
        }
      }
      if(lhs.state == SimplificationState::head_closed) {
        auto asymmetric = get_asymmetric_explode_from_equation(rhs.expression, lhs.expression, variables);
        if(std::holds_alternative<AsymmetricHeadFailure>(asymmetric)) return AttemptResult::failed;
        if(auto* success = std::get_if<AsymmetricExplodeSpec>(&asymmetric)) {
          process_asymmetric_explosion(index, stack, std::move(*success));
          return AttemptResult::handled;
        }
      }
      return AttemptResult::nothing;
    }
    AttemptResult try_to_judge_equal(std::uint64_t index, expression::Stack stack, Simplification const& lhs, Simplification const& rhs) {
      if(lhs.expression == rhs.expression) {
        return AttemptResult::handled;
      } else {
        return AttemptResult::nothing;
      }
    }
    bool examine_equation(std::uint64_t index) {
      auto& equation = equations[index];
      auto lhs = context.simplify(std::move(equation.lhs));
      auto rhs = context.simplify(std::move(equation.rhs));

      AttemptResult ret = AttemptResult::nothing;
      std::apply([&](auto... tests) {
        (is_definitive(ret = (this->*tests)(index, equation.stack, lhs, rhs)) || ...);
      }, std::make_tuple(
        &Impl::try_to_extract_rule,
        &Impl::try_to_deepen,
        &Impl::try_to_explode_symmetric,
        &Impl::try_to_explode_asymmetric,
        &Impl::try_to_judge_equal)
      );
      auto& equation_final = equations[index]; //Vector might move!!!
      equation_final.lhs = std::move(lhs.expression);
      equation_final.rhs = std::move(rhs.expression);
      if(ret == AttemptResult::handled) {
        equation_final.handled = true;
        return true;
      } else if(ret == AttemptResult::failed) {
        equation_final.failed = true;
        return true;
      } else {
        return false;
      }
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
    void register_variable(std::uint64_t index) {
      variables.insert(index);
    }
    std::uint64_t add_equation(expression::Stack stack, tree::Expression lhs, tree::Expression rhs) {
      auto ret = equations.size();
      equations.push_back({
        .stack = stack,
        .lhs = std::move(lhs),
        .rhs = std::move(rhs)
      });
      return ret;
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
    bool is_fully_satisfied() {
      for(auto const& equation : equations) {
        if(!equation.handled) return false;
      }
      return true;
    }
  };
  Solver::Solver(Context context):impl(new Impl{.context = std::move(context)}) {}
  void Solver::register_variable(std::uint64_t index) {
    return impl->register_variable(index);
  }
  std::uint64_t Solver::add_equation(expression::Stack stack, tree::Expression lhs, tree::Expression rhs) {
    return impl->add_equation(std::move(stack), std::move(lhs), std::move(rhs));
  }
  bool Solver::is_equation_satisfied(std::uint64_t index) {
    return impl->is_equation_satisfied(index);
  }
  bool Solver::is_fully_satisfied() {
    return impl->is_fully_satisfied();
  }
  bool Solver::try_to_make_progress() {
    return impl->try_to_make_progress();
  }
  std::vector<std::pair<tree::Expression, tree::Expression> > Solver::get_equations() {
    std::vector<std::pair<tree::Expression, tree::Expression> > ret;
    for(auto const& equation : impl->equations) {
      if(equation.handled) continue;
      ret.emplace_back(equation.lhs, equation.rhs);
    }
    return ret;
  }
  Solver::Solver(Solver&&) = default;
  Solver& Solver::operator=(Solver&&) = default;
  Solver::~Solver() = default;

}
