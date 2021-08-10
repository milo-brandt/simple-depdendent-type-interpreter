/*
Say that an expression is head-closed if there can be no replacements affecting
its left hand spine, even if more arguments are applied and even if variables
receive definitions.

Solving algorithm: repeatedly loop over all equations (as long as progress is being made), and apply the following steps.

Reduce both sides of the equation fully, then to see if progress can be made by
applying any of the following rules. The first rule that works should be applied
and then no further rules should be checked until the next time the equation is
handled.

  I. Definition Extraction
          VARIABLE arg(5) arg(3) arg(1) = f arg(3) (arg(1) arg(5))
      If one side is a variable with a series of distinct arguments
      applied to and the other side...
        i.  Only uses arguments present on the left.
        ii. Does not depend in any way on VARIABLE being defined. (Including through rules)
      ...then, a new rule should be created defining VARIABLE as a lambda and the
      equation should be marked as solved.

  II. Deepening
          F = G
      If one side of the equation would reduce further if more arguments were applied
      to it, then a new argument should be introduced and applied to both sides.

  III. Symmetric Explosion
          add x y = add z w
      If both sides are head-closed and have the same head and same number of arguments,
      create new equations expressing the equality of corresponding arguments of the
      expression and mark the current equation as solved.

      If both sides are head-closed, but have different heads or different numbers of
      arguments, mark the equation as unsolvable.

  IV. Asymmetric Explosion
          VARIABLE arg(0) arg(1) = add (f arg(0)) (g arg(1))
      If one side is a variable with a series of distinct arguments applied and the
      other side is head-closed and the head is either an external or one of the
      arguments applied to the variable, then...
        i.   Introduce a new variable for each argument on the head-closed side.
        ii.  Define VARIABLE to be the head applied to the new introductions
             (with appropriate arguments applied)
        iii. Create a new equation expressing that these variables equal the
             corresponding argument of the head-closed side.
      Finally, mark the current equation as solved.

  V.  Argument Reduction
          VARIABLE arg(0) = OTHER_VARIABLE arg(1)
      If one side is a variable with a series of distinct arguments applied and the
      other side is independent of one or more of those variables, introduce a new
      variable. Add a rule which asserts that VARIABLE with each argument applied
      equals the introduced variable with only the arguments present on both sides
      applied and add a new equation asserting that this new variable with the common
      arguments applied equals the second side.

  VI. Judgmental Equality
          VARIABLE = VARIABLE
      If the two sides are literally equal, mark the equation as solved.
*/

#ifndef EXPRESSION_SOLVER_HPP
#define EXPRESSION_SOLVER_HPP

#include "evaluation_context.hpp"
#include "expression_tree.hpp"
#include <vector>
#include <unordered_set>
#include <optional>
#include <unordered_map>
#include <iostream>

namespace expression::solve {
  enum class EquationSide {
    lhs, rhs
  };
  namespace equation_explanation {
    struct External {};
    struct Deepen {
      std::uint64_t equation_index;
    };
    struct Exploded {
      std::uint64_t equation_index;
      std::uint64_t part_index;
    };
    struct ArgEliminate {
      std::uint64_t equation_index;
    };
    struct FromCast {
      std::uint64_t equation_index;
    };
    using Any = std::variant<External, Deepen, Exploded, ArgEliminate, FromCast>;
  };
  namespace introduction_explanation {
    struct Exploded {
      std::uint64_t equation_index;
      std::uint64_t part_index;
    };
    struct ArgEliminated {
      std::uint64_t equation_index;
    };
    using Any = std::variant<Exploded, ArgEliminated>;
  };
  namespace rule_explanation {
    struct FromEquation {
      std::uint64_t equation;
    };
    struct FromCast {
      std::uint64_t equation;
    };
    using Any = std::variant<FromEquation, FromCast>;
  };
  namespace equation_disposition {
    struct Open {};
    struct AxiomMismatch {};
    struct ArgumentMismatch {};
    struct Rewritten { //Success (elimination)
      EquationSide pattern_side;
    };
    struct Deepened {
      std::uint64_t derived_index;
    }; //Success (transformation)
    struct SymmetricExplode {
      std::variant<tree::External, tree::Arg> head;
      std::uint64_t arg_count;
      std::uint64_t first_derived_index;
    };
    struct AsymmetricExplode {
      EquationSide pattern_side;
      std::uint64_t pattern_head;
      std::uint64_t pattern_args;
      std::variant<tree::External, tree::Arg> replacement_head;
      std::uint64_t replacement_args;
      std::uint64_t first_derived_index;
    };
    struct JudgedTrue {};
    /*struct ArgEliminated {
      EquationSide excess_side; //the side that used excess arguments
      std::vector<std::uint64_t> eliminations;
    };*/
    using Any = std::variant<Open, AxiomMismatch, ArgumentMismatch, Rewritten, Deepened, SymmetricExplode, AsymmetricExplode, JudgedTrue>;
  };

  template<class T>
  concept SolverContext = requires(
    T context,
    tree::Tree term,
    pattern::Tree pattern,
    introduction_explanation::Any intro_reason,
    rule_explanation::Any rule_reason,
    std::uint64_t ext_index,
    Rule rule,
    std::unordered_set<std::uint64_t> const& vars
  ) {
    context.reduce(std::move(term));                //reduce a term fully
    context.needs_more_arguments(term);             //return whether equality for the term should be tested with added dummy args (i.e. if it's a function)
    context.is_head_closed(term, vars);                   //return whether the head can be trusted not to further reduce
    context.add_rule(rule, rule_reason);            //(void)
    context.introduce_variable(intro_reason);       //return std::uint64_t for new term
    context.term_depends_on(ext_index, ext_index);  //determine whether one index depends on another
  };

  struct SolveFragment {
    std::uint64_t depth;
    tree::Tree lhs;
    tree::Tree rhs;
    equation_disposition::Any disposition;
    equation_explanation::Any explanation;
  };

  struct SolveResult {
    std::vector<SolveFragment> parts;
  };

  struct EquationSpecification {
    std::uint64_t depth;
    tree::Tree lhs;
    tree::Tree rhs;
  };

  template<SolverContext Context>
  class Solver {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Solver(Context);
    std::vector<SolveFragment> const& parts();
    std::unordered_set<std::uint64_t> const& variables();
    std::uint64_t add_equation(EquationSpecification);
    void register_variable(std::uint64_t);
    bool is_equation_satisfied(std::uint64_t index) const;
    bool is_fully_satisfied() const;
    bool deduce(); //return true if any progress was made
  };
  template<class Context> Solver(Context&&) -> Solver<Context>;

  template<class Context> Solver<Context>::Solver(Context context):impl(std::make_unique<Impl>(std::forward<Context>(context))){}
  template<class Context> std::vector<SolveFragment> const& Solver<Context>::parts() { return impl->parts; }
  template<class Context> std::unordered_set<std::uint64_t> const& Solver<Context>::variables() { return impl->variables; }
  template<class Context> std::uint64_t Solver<Context>::add_equation(EquationSpecification specification) { return impl->add_equation(std::move(specification)); }
  template<class Context> void Solver<Context>::register_variable(std::uint64_t var) { return impl->register_variable(var); }
  template<class Context> bool Solver<Context>::is_equation_satisfied(std::uint64_t index) const { return impl->is_equation_satisfied(index); }
  template<class Context> bool Solver<Context>::is_fully_satisfied() const { return impl->is_fully_satisfied(); }
  template<class Context> bool Solver<Context>::deduce() { return impl->deduce(); }

  template<class Context>
  struct Solver<Context>::Impl {
    struct CastInfo {
      std::uint64_t index;
      tree::Tree source;
      std::uint64_t cast_result;
      std::uint64_t depth;
    };
    Context context;
    std::vector<SolveFragment> parts;
    std::unordered_set<std::uint64_t> variables;
    Impl(Context context):context(std::forward<Context>(context)) {}
    /*
      Utilities
    */
    enum AttemptResult {
      nothing,
      made_progress,
      failed
    };
    bool is_result_definitive(AttemptResult result) {
      return result != nothing;
    }
    struct DefinitionFormData {
      std::uint64_t head;                                               //checked to be variable
      std::vector<std::uint64_t> arg_list;                              //argument at each index
      std::unordered_map<std::uint64_t, std::uint64_t> arg_to_index;    //which arguments appear at which indices
    };
    std::optional<DefinitionFormData> is_term_in_definition_form(tree::Tree const& term) {
      auto unfolded = unfold_ref(term);
      if(auto const* ext = unfolded.head->get_if_external()) {
        if(variables.contains(ext->index)) {
          DefinitionFormData ret{
            .head = ext->index
          };
          for(std::uint64_t index = 0; index < unfolded.args.size(); ++index) {
            if(auto const* arg_data = unfolded.args[index]->get_if_arg()) {
              if(ret.arg_to_index.contains(arg_data->index)) {
                return std::nullopt; //a single argument is applied twice
              } else {
                ret.arg_list.push_back(arg_data->index);
                ret.arg_to_index.insert(std::make_pair(arg_data->index, index));
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
    pattern::Tree make_pattern_for(std::uint64_t head, std::size_t arg_count) {
      pattern::Tree ret = pattern::Fixed{head};
      for(std::size_t i = 0; i < arg_count; ++i) {
        ret = pattern::Apply{std::move(ret), pattern::Wildcard{}};
      }
      return ret;
    }
    tree::Tree apply_args_enumerated(tree::Tree head, std::uint64_t arg_count) {
      tree::Tree ret = std::move(head);
      for(std::uint64_t i = 0; i < arg_count; ++i) {
        ret = tree::Apply{std::move(ret), tree::Arg{i}};
      }
      return ret;
    }
    tree::Tree apply_args_vector(tree::Tree head, std::vector<std::uint64_t> const& args) {
      tree::Tree ret = std::move(head);
      for(auto arg : args) {
        ret = tree::Apply{std::move(ret), tree::Arg{arg}};
      }
      return ret;
    }
    std::variant<tree::External, tree::Arg> get_head_variant(tree::Tree term) {
      if(auto* arg = term.get_if_arg()) {
        return std::move(*arg);
      } else {
        return std::move(term.get_external());
      }
    }
    /*
      I. Definition Extraction
    */
    std::optional<tree::Tree> get_replacement_from(std::unordered_map<std::uint64_t, std::uint64_t> const& map, std::uint64_t pattern_head, tree::Tree const& term) {
      return term.visit(mdb::overloaded{
        [&](tree::Apply const& apply) -> std::optional<tree::Tree> {
          if(auto lhs = get_replacement_from(map, pattern_head, apply.lhs)) {
            if(auto rhs = get_replacement_from(map, pattern_head, apply.rhs)) {
              return tree::Apply{std::move(*lhs), std::move(*rhs)};
            }
          }
          return std::nullopt;
        },
        [&](tree::Arg const& arg) -> std::optional<tree::Tree> {
          if(map.contains(arg.index)) {
            return tree::Arg{map.at(arg.index)};
          } else {
            return std::nullopt;
          }
        },
        [&](tree::External const& ext) -> std::optional<tree::Tree> {
          if(context.term_depends_on(ext.index, pattern_head)) {
            return std::nullopt;
          } else {
            return tree::Tree{ext};
          }
        }
      });
    }
    std::optional<Rule> get_rule_from_equation(tree::Tree const& lhs, tree::Tree const& rhs) {
      if(auto map = is_term_in_definition_form(lhs)) {
        if(auto tree = get_replacement_from(map->arg_to_index, map->head, rhs)) {
          variables.erase(map->head);
          return Rule{
            .pattern = make_pattern_for(map->head, map->arg_to_index.size()),
            .replacement = std::move(*tree)
          };
        }
      }
      return std::nullopt;
    }
    AttemptResult try_to_extract_rule(std::uint64_t index, SolveFragment& fragment) {
      if(auto rule = get_rule_from_equation(fragment.lhs, fragment.rhs)) {
        fragment.disposition = equation_disposition::Rewritten{
          .pattern_side = EquationSide::lhs
        };
        context.add_rule(std::move(*rule), rule_explanation::FromEquation{index});
        return made_progress;
      }
      if(auto rule = get_rule_from_equation(fragment.rhs, fragment.lhs)) {
        fragment.disposition = equation_disposition::Rewritten{
          .pattern_side = EquationSide::rhs
        };
        context.add_rule(std::move(*rule), rule_explanation::FromEquation{index});
        return made_progress;
      }
      return nothing;
    }
    /*
      II. Deepening
    */
    AttemptResult try_to_deepen(std::uint64_t index, SolveFragment& fragment) {
      if(context.needs_more_arguments(fragment.lhs) || context.needs_more_arguments(fragment.rhs)) {
        fragment.disposition = equation_disposition::Deepened{
          .derived_index = parts.size()
        };
        parts.push_back({
          .depth = fragment.depth + 1,
          .lhs = tree::Apply{fragment.lhs, tree::Arg{fragment.depth}},
          .rhs = tree::Apply{fragment.rhs, tree::Arg{fragment.depth}},
          .disposition = equation_disposition::Open{},
          .explanation = equation_explanation::Deepen{index}
        });
        return made_progress;
      } else {
        return nothing;
      }
    }
    /*
      III. Symmetric Explosion
    */
    AttemptResult try_to_explode_symmetric(std::uint64_t index, SolveFragment& fragment) {
      if(context.is_head_closed(fragment.lhs, variables) && context.is_head_closed(fragment.rhs, variables)) {
        auto unfold_lhs = unfold(fragment.lhs);
        auto unfold_rhs = unfold(fragment.rhs);
        if(unfold_lhs.head == unfold_rhs.head && unfold_lhs.args.size() == unfold_rhs.args.size()) {
          fragment.disposition = equation_disposition::SymmetricExplode{
            .head = get_head_variant(unfold_lhs.head),
            .arg_count = unfold_lhs.args.size(),
            .first_derived_index = parts.size()
          };
          for(std::uint64_t i = 0; i < unfold_lhs.args.size(); ++i) {
            parts.push_back({
              .depth = fragment.depth,
              .lhs = unfold_lhs.args[i],
              .rhs = unfold_rhs.args[i],
              .disposition = equation_disposition::Open{},
              .explanation = equation_explanation::Exploded{
                .equation_index = index,
                .part_index = i
              }
            });
          }
          return made_progress;
        } else {
          fragment.disposition = equation_disposition::AxiomMismatch{};
          return failed;
        }
      } else {
        return nothing;
      }
    }
    /*
      IV. Asymmetric Explosion
    */
    struct AsymmetricExplodeSpec {
      std::uint64_t pattern_head;
      std::vector<std::uint64_t> pattern_args;
      std::variant<tree::External, tree::Arg> irreducible_head; //if arg, gives proper position for pattern
      std::vector<tree::Tree> irreducible_args;
    };
    struct AsymmetricHeadFailure {};
    std::variant<std::monostate, AsymmetricHeadFailure, AsymmetricExplodeSpec> get_asymmetric_explode_from_equation(tree::Tree const& lhs, tree::Tree const& rhs) {
      if(auto def_form = is_term_in_definition_form(lhs)) {
        if(context.is_head_closed(rhs, variables)) {
          auto unfolded = unfold(rhs);

          AsymmetricExplodeSpec ret{
            .pattern_head = def_form->head,
            .pattern_args = std::move(def_form->arg_list),
            .irreducible_args = std::move(unfolded.args)
          };

          if(auto* head_arg = unfolded.head.get_if_arg()) {
            if(!def_form->arg_to_index.contains(head_arg->index)) {
              return AsymmetricHeadFailure{}; //illegal!
            } else {
              ret.irreducible_head = tree::Arg{def_form->arg_to_index.at(head_arg->index)};
            }
          } else {
            ret.irreducible_head = unfolded.head.get_external();
          }
          return ret;
        }
      }
      return std::monostate{};
    }
    void process_asymmetric_explosion(std::uint64_t equation_index, std::uint64_t depth, AsymmetricExplodeSpec spec) {
      tree::Tree replacement = std::visit([](auto const& v) -> tree::Tree { return v; }, spec.irreducible_head);

      for(std::uint64_t i = 0; i < spec.irreducible_args.size(); ++i) {
        auto var = context.introduce_variable(introduction_explanation::Exploded{
          .equation_index = equation_index,
          .part_index = i
        });
        variables.insert(var);
        replacement = tree::Apply{std::move(replacement), apply_args_enumerated(tree::External{var}, spec.pattern_args.size())};
        parts.push_back({
          .depth = depth,
          .lhs = apply_args_vector(tree::External{var}, spec.pattern_args),
          .rhs = spec.irreducible_args[i],
          .disposition = equation_disposition::Open{},
          .explanation = equation_explanation::Exploded{
            .equation_index = equation_index,
            .part_index = i
          }
        });
      }

      variables.erase(spec.pattern_head);
      context.add_rule({
        .pattern = make_pattern_for(spec.pattern_head, spec.pattern_args.size()),
        .replacement = std::move(replacement)
      }, rule_explanation::FromEquation{equation_index});

      variables.erase(spec.pattern_head);
    }
    AttemptResult try_to_explode_asymmetric(std::uint64_t equation_index, SolveFragment& fragment) {
      {
        auto lhs_spec = get_asymmetric_explode_from_equation(fragment.lhs, fragment.rhs);
        if(auto* explode = std::get_if<AsymmetricExplodeSpec>(&lhs_spec)) {
          fragment.disposition = equation_disposition::AsymmetricExplode{
            .pattern_side = EquationSide::lhs,
            .pattern_head = explode->pattern_head,
            .pattern_args = explode->pattern_args.size(),
            .replacement_head = explode->irreducible_head,
            .replacement_args = explode->irreducible_args.size(),
            .first_derived_index = parts.size()
          };
          process_asymmetric_explosion(equation_index, fragment.depth, std::move(*explode));
          return made_progress;
        } else if(std::holds_alternative<AsymmetricHeadFailure>(lhs_spec)) {
          fragment.disposition = equation_disposition::ArgumentMismatch{};
          return failed;
        }
      }
      {
        auto rhs_spec = get_asymmetric_explode_from_equation(fragment.rhs, fragment.lhs);
        if(auto* explode = std::get_if<AsymmetricExplodeSpec>(&rhs_spec)) {
          fragment.disposition = equation_disposition::AsymmetricExplode{
            .pattern_side = EquationSide::rhs,
            .pattern_head = explode->pattern_head,
            .pattern_args = explode->pattern_args.size(),
            .replacement_head = explode->irreducible_head,
            .replacement_args = explode->irreducible_args.size(),
            .first_derived_index = parts.size()
          };
          process_asymmetric_explosion(equation_index, fragment.depth, std::move(*explode));
          return made_progress;
        } else if(std::holds_alternative<AsymmetricHeadFailure>(rhs_spec)) {
          fragment.disposition = equation_disposition::ArgumentMismatch{};
          return failed;
        }
      }
      return nothing;
    }
    /*
      V. Argument Reduction
    */
    /*
    void append_arguments_in(tree::Tree const& term, std::unordered_set<std::uint64_t>& set) {
      term.visit(mdb::overloaded{
        [&](tree::Apply const& apply) {
          append_arguments_in(apply.lhs, set);
          append_arguments_in(apply.rhs, set);
        },
        [&](tree::Arg const& arg) {
          set.insert(arg.index);
        },
        [&](tree::External const&) {}
      });
    }
    std::unordered_set<std::uint64_t> get_arguments_in(tree::Tree const& term) {
      std::unordered_set<std::uint64_t> ret;
      append_arguments_in(term, ret);
      return ret;
    }
    bool try_to_reduce_arguments(std::uint64_t equation_index, SolveFragment& fragment) {
    }
*/
    /*
      VI. Judgmental Equality
    */
    AttemptResult try_to_judge_equal(std::uint64_t, SolveFragment& fragment) {
      if(fragment.lhs == fragment.rhs) {
        fragment.disposition = equation_disposition::JudgedTrue{};
        return made_progress;
      } else {
        return nothing;
      }
    }

    bool examine_equation(std::size_t index) {
      //0 <= index < parts.size() + disposition is "open"
      auto& fragment = parts[index];
      fragment.lhs = context.reduce(fragment.lhs);
      fragment.rhs = context.reduce(fragment.rhs);

      return is_result_definitive(try_to_extract_rule(index, fragment))
          || is_result_definitive(try_to_deepen(index, fragment))
          || is_result_definitive(try_to_explode_symmetric(index, fragment))
          || is_result_definitive(try_to_explode_asymmetric(index, fragment))
          || is_result_definitive(try_to_judge_equal(index, fragment));
    }
    /*
      Interface
    */
    std::uint64_t add_equation(EquationSpecification specification) {
      std::uint64_t ret = parts.size();
      parts.push_back({
        .depth = specification.depth,
        .lhs = std::move(specification.lhs),
        .rhs = std::move(specification.rhs),
        .disposition = equation_disposition::Open{},
        .explanation = equation_explanation::External{}
      });
      return ret;
    }
    void register_variable(std::uint64_t var) {
      variables.insert(var);
    }
    bool is_equation_satisfied(std::uint64_t index) const {
      std::vector<std::uint64_t> to_check;
      to_check.push_back(index);
      while(!to_check.empty()) {
        auto current = to_check.back();
        to_check.pop_back();
        bool failed = false;
        std::visit(mdb::overloaded{
          [&](equation_disposition::Open const&) {
            failed = true;
          },
          [&](equation_disposition::AxiomMismatch const&) {
            failed = true;
          },
          [&](equation_disposition::ArgumentMismatch const&) {
            failed = true;
          },
          [&](equation_disposition::Rewritten const&) {}, //okay (no derived checks)
          [&](equation_disposition::Deepened const& deepened) {
            to_check.push_back(deepened.derived_index);
          },
          [&](equation_disposition::SymmetricExplode const& explode) {
            for(std::uint64_t i = 0; i < explode.arg_count; ++i) {
              to_check.push_back(explode.first_derived_index + i);
            }
          },
          [&](equation_disposition::AsymmetricExplode const& explode) {
            for(std::uint64_t i = 0; i < explode.replacement_args; ++i) {
              to_check.push_back(explode.first_derived_index + i);
            }
          },
          [&](equation_disposition::JudgedTrue const&) {}, //okay (no derived checks)
        }, parts[current].disposition);
        if(failed) return false;
      }
      return true;
    }
    bool is_fully_satisfied() const {
      for(auto const& fragment : parts) {
        if(!std::visit(mdb::overloaded{ //return false to fail
          [&](equation_disposition::Open const&) { return false; },
          [&](equation_disposition::AxiomMismatch const&) { return false; },
          [&](equation_disposition::ArgumentMismatch const&) { return false; },
          [&](equation_disposition::Rewritten const&) { return true; },
          [&](equation_disposition::Deepened const&) { return true; },
          [&](equation_disposition::SymmetricExplode const&) { return true; },
          [&](equation_disposition::AsymmetricExplode const&) { return true; },
          [&](equation_disposition::JudgedTrue const&) { return true; }
        }, fragment.disposition)) {
          return false;
        }
      }
      return true;
    }
    bool deduce() {
      bool made_progress = false;
      std::uint64_t start_point = 0;
    DEDUCE_START:
      for(std::uint64_t i = start_point; i < parts.size(); ++i) {
        if(std::holds_alternative<equation_disposition::Open>(parts[i].disposition)) {
          if(examine_equation(i)) {
            made_progress = true;
            start_point = i + 1;
            goto DEDUCE_START;
          }
        }
      }
      for(std::uint64_t i = 0; i < start_point; ++i) {
        if(std::holds_alternative<equation_disposition::Open>(parts[i].disposition)) {
          if(examine_equation(i)) {
            made_progress = true;
            start_point = i + 1;
            goto DEDUCE_START;
          }
        }
      }
      return made_progress;
    }
  };

}

#endif
