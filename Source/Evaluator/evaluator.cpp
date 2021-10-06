#include "evaluator.hpp"
#include "../Expression/expression_debug_format.hpp"

namespace evaluator {
  namespace {
    void infect_axioms(Instance& instance, expression::tree::Expression expr, std::uint64_t mark) {
      auto& mark_set = instance.marks[expr];
      if(mark_set.contains(mark)) return; //already infected.
      mark_set.insert(mark);
      auto unfolding = expression::unfold(expr);
      if(auto* ext = unfolding.head.get_if_external()) {
        if(instance.is_external_axiom[ext->external_index]) {
          for(auto arg : unfolding.args) {
            infect_axioms(instance, arg, mark); //If x = axiom y_1 y_2 y_3, then y_i < x for each i.
          }
        }
      }
    }
    expression::tree::Expression infectious_substitute_into_replacement(Instance& instance, std::vector<expression::tree::Expression> const& terms, expression::tree::Expression const& replacement) {
      auto unfolding = expression::unfold(replacement);
      if(auto* arg_head = unfolding.head.get_if_arg()) { //might be marked!
        auto term_head = terms.at(arg_head->arg_index);
        if(instance.marks.contains(term_head)) {
          auto& mark_set = instance.marks.at(term_head);
          auto ext_head = expression::unfold(term_head).head.get_if_external();
          bool is_axiom_head = ext_head && instance.is_external_axiom[ext_head->external_index];
          for(auto& arg : unfolding.args) {
            auto new_arg = infectious_substitute_into_replacement(instance, terms, std::move(arg));
            if(is_axiom_head) {
              //infect the new argument to an infected axiom
              for(auto mark : mark_set) {
                infect_axioms(instance, new_arg, mark); //infect the new argument (to an axiom)
              }
            }
            unfolding.head = expression::tree::Apply{
              std::move(unfolding.head),
              std::move(new_arg)
            };
            auto& apply_mark_set = instance.marks[unfolding.head];
            for(auto mark : mark_set) {
              apply_mark_set.insert(mark); //infect the application
            }
          }
          return std::move(unfolding.head);
        }
      }
      //just refold normally.
      for(auto& arg : unfolding.args) {
        unfolding.head = expression::tree::Apply{
          std::move(unfolding.head),
          infectious_substitute_into_replacement(instance, terms, std::move(arg))
        };
      }
      return unfolding.head;
    }
  }
  expression::tree::Expression Instance::reduce(expression::tree::Expression expr, std::size_t ignore_replacement) {
    struct Detail {
      Instance& me;
      std::size_t ignore_replacement;
      std::unordered_map<expression::tree::Expression, expression::tree::Expression, TreeHasher> memoized;
      expression::tree::Expression reduce(expression::tree::Expression input) {
        if(memoized.contains(input)) return memoized.at(input);
        std::vector<expression::tree::Expression> steps;
        auto ret = [&] {
          expression::tree::Expression target = input;
        RESTART_REDUCTION:
          if(memoized.contains(target)) return memoized.at(target);
          steps.push_back(target);
          if(auto* apply = target.get_if_apply()) {
            target = expression::tree::Apply{
              reduce(apply->lhs),
              reduce(apply->rhs)
            };
          }
          for(auto const& rule : me.rules) {
            if(expression::term_matches(target, rule.pattern)) {
              auto args = expression::destructure_match(target, rule.pattern);
              target = expression::substitute_into_replacement(std::move(args), rule.replacement);
              goto RESTART_REDUCTION;
            }
          }
          for(auto const& data_rule : me.data_rules) {
            if(expression::term_matches(target, data_rule.pattern)) {
              auto args = expression::destructure_match(target, data_rule.pattern);
              target = data_rule.replace(std::move(args));
              goto RESTART_REDUCTION;
            }
          }
          std::size_t replacement_index = 0;
          for(auto const& replacement : me.replacements) {
            if(replacement_index++ == ignore_replacement) continue;
            if(replacement.pattern == target) {
              if(replacement.associated_mark != -1) {
                if(me.marks.contains(target)) {
                  if(me.marks.at(target).contains(replacement.associated_mark)) {
                    std::terminate(); //loop
                  }
                  for(auto mark : me.marks.at(target)) {
                    infect_axioms(me, replacement.replacement, mark);
                  }
                }
              }
              target = replacement.replacement;
              goto RESTART_REDUCTION;
            }
          }
          return target;
        }();
        for(auto& step : steps) {
          memoized.insert(std::make_pair(std::move(step), ret));
        }
        return ret;
      }
    };
    return Detail{*this, ignore_replacement}.reduce(expr);
  }
  bool Instance::is_purely_open(expression::tree::Expression expr) {
    if(is_axiomatic(expr)) return false;
    for(auto const& rule : rules) {
      auto* pattern = &rule.pattern;
      while(auto* apply = pattern->get_if_apply()) {
        pattern = &apply->lhs;
        if(expression::term_matches(expr, *pattern)) return false;
      }
    }
    for(auto const& rule : data_rules) {
      auto* pattern = &rule.pattern;
      while(auto* apply = pattern->get_if_apply()) {
        pattern = &apply->lhs;
        if(expression::term_matches(expr, *pattern)) return false;
      }
    }
    /*
    for(auto const& replacement : replacements) {
      if(replacement.second.holds_conglomerate())
        continue; //this value doesn't have computational meaning anyways
      auto* pattern = &replacement.first;
      while(auto* apply = pattern->get_if_apply()) {
        pattern = &apply->lhs;
        if(expr == *pattern) return false;
      }
    }
    Not sure whether this is the right thing? These problems should be detected
    elsewhere (since it involves replacement.first containing another)
    */
    return true;
  }
  bool Instance::is_axiomatic(expression::tree::Expression reduced_expr) {
    auto unfolding = expression::unfold(reduced_expr);
    if(auto* ext = unfolding.head.get_if_external()) {
      return is_external_axiom[ext->external_index];
    } else {
      return false;
    }
  }
  Instance Instance::create(
    std::vector<bool> is_external_axiom,
    std::vector<expression::Rule> rules,
    std::vector<expression::DataRule> data_rules,
    std::vector<Replacement> replacements
  ) {
    Instance ret{
      .is_external_axiom = std::move(is_external_axiom),
      .rules = std::move(rules),
      .data_rules = std::move(data_rules)
    };
    std::uint64_t conglomerate_index = 0;
    std::vector<Replacement> proposed_replacements;
    for(;conglomerate_index < replacements.size(); ++conglomerate_index) {
      proposed_replacements.push_back({
        replacements[conglomerate_index].pattern,
        expression::tree::Conglomerate{conglomerate_index},
        conglomerate_index
      });
      proposed_replacements.push_back({
        replacements[conglomerate_index].replacement,
        expression::tree::Conglomerate{conglomerate_index},
        conglomerate_index
      });
    }
  RESTART_REDUCTION:
    std::cout << "-------------\n";
    for(auto& replacement : ret.replacements) {
      std::cout << "[" << replacement.associated_mark << "] " << expression::raw_format(replacement.pattern) << " -> " << expression::raw_format(replacement.replacement) << "\n";
    }
    for(auto& replacement : proposed_replacements) {
      std::cout << "[" << replacement.associated_mark << "] " << expression::raw_format(replacement.pattern) << " -> " << expression::raw_format(replacement.replacement) << " [Proposed]\n";
    }

    for(std::size_t inspection_pos = 0; inspection_pos < ret.replacements.size() || !proposed_replacements.empty(); ++inspection_pos) {
      bool newly_inspected = false;
      if(inspection_pos == ret.replacements.size()) {
        newly_inspected = true;
        ret.replacements.push_back(proposed_replacements.front());
        proposed_replacements.erase(proposed_replacements.begin());
      }
      auto& replacement = ret.replacements[inspection_pos];
      replacement.replacement = ret.reduce(replacement.replacement, inspection_pos);
      if(replacement.pattern.holds_conglomerate()) {
        continue;
      } //keep conglomerate definitions intact
      auto reduced_lhs = ret.reduce(replacement.pattern, inspection_pos);
      if(newly_inspected || replacement.pattern != reduced_lhs) {
        if(reduced_lhs.get_if_conglomerate()) {
          replacement.pattern = std::move(reduced_lhs); //okay, defining conglomerate
          if(replacement.pattern == replacement.replacement) { //should be more general check of dependence rather than equality...
            auto vec_index = &replacement - ret.replacements.data();
            ret.replacements.erase(ret.replacements.begin() + vec_index);
          } else {
            infect_axioms(ret, replacement.replacement, replacement.associated_mark);
          }
          goto RESTART_REDUCTION;
        } else if(ret.is_axiomatic(reduced_lhs)) {
          if(replacement.replacement.holds_conglomerate()) {
            //define conglomerate to be this new value
            replacement.pattern = std::move(replacement.replacement);
            replacement.replacement = std::move(reduced_lhs);
            infect_axioms(ret, replacement.replacement, replacement.associated_mark);
            goto RESTART_REDUCTION;
          } else {
            auto unfolded_lhs = expression::unfold(reduced_lhs);
            auto unfolded_rhs = expression::unfold(replacement.replacement);
            if(unfolded_lhs.head != unfolded_rhs.head || unfolded_lhs.args.size() != unfolded_rhs.args.size()) {
              std::terminate(); //mismatched axioms
            }
            for(std::size_t i = 0; i < unfolded_lhs.args.size(); ++i, ++conglomerate_index) {
              proposed_replacements.emplace_back(
                unfolded_lhs.args[i],
                expression::tree::Conglomerate{conglomerate_index},
                conglomerate_index
              );
              proposed_replacements.emplace_back(
                unfolded_rhs.args[i],
                expression::tree::Conglomerate{conglomerate_index},
                conglomerate_index
              );
            }
            auto vec_index = &replacement - ret.replacements.data();
            ret.replacements.erase(ret.replacements.begin() + vec_index);
            goto RESTART_REDUCTION;
          }
        } else if(ret.is_purely_open(reduced_lhs)) {
          if(!newly_inspected || replacement.pattern != reduced_lhs) {
            replacement.pattern = std::move(reduced_lhs);
            goto RESTART_REDUCTION; //okay, term remains open.
          }
          //can continue if this is a new inspection that turned up nothing.
        } else {
          std::terminate(); //Something weird with computational behavior got in.
        }
      }
    }
    std::cout << "-------------\n";
    for(auto& replacement : ret.replacements) {
      std::cout << expression::raw_format(replacement.pattern) << " -> " << expression::raw_format(replacement.replacement) << "\n";
    }
    return ret;
  }
}
