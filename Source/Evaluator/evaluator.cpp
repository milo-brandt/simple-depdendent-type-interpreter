#include "evaluator.hpp"
#include "../Expression/expression_debug_format.hpp"

namespace evaluator {
  expression::tree::Expression Instance::reduce(expression::tree::Expression expr, std::size_t ignore_replacement) {
    struct TreeHasher {
      std::hash<void const*> h;
      auto operator()(expression::tree::Expression const& expr) const noexcept {
        return h(expr.data());
      }
    };
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
            if(replacement.first == target) {
              target = replacement.second;
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
    std::vector<std::pair<expression::tree::Expression, expression::tree::Expression> > replacements
  ) {
    Instance ret{
      .is_external_axiom = std::move(is_external_axiom),
      .rules = std::move(rules),
      .data_rules = std::move(data_rules)
    };
    std::uint64_t conglomerate_index = 0;
    std::vector<std::pair<expression::tree::Expression, expression::tree::Expression> > proposed_replacements;
    for(;conglomerate_index < replacements.size(); ++conglomerate_index) {
      proposed_replacements.emplace_back(
        replacements[conglomerate_index].first,
        expression::tree::Conglomerate{conglomerate_index}
      );
      proposed_replacements.emplace_back(
        replacements[conglomerate_index].second,
        expression::tree::Conglomerate{conglomerate_index}
      );
    }
  RESTART_REDUCTION:
    std::cout << "-------------\n";
    for(auto& replacement : ret.replacements) {
      std::cout << expression::raw_format(replacement.first) << " -> " << expression::raw_format(replacement.second) << "\n";
    }
    for(auto& replacement : proposed_replacements) {
      std::cout << expression::raw_format(replacement.first) << " -> " << expression::raw_format(replacement.second) << " [Proposed]\n";
    }

    for(std::size_t inspection_pos = 0; inspection_pos < ret.replacements.size() || !proposed_replacements.empty(); ++inspection_pos) {
      bool newly_inspected = false;
      if(inspection_pos == ret.replacements.size()) {
        newly_inspected = true;
        ret.replacements.push_back(proposed_replacements.front());
        proposed_replacements.erase(proposed_replacements.begin());
      }
      auto& replacement = ret.replacements[inspection_pos];
      replacement.second = ret.reduce(replacement.second, inspection_pos);
      if(replacement.first.holds_conglomerate()) {
        continue;
      } //keep conglomerate definitions intact
      auto reduced_lhs = ret.reduce(replacement.first, inspection_pos);
      if(newly_inspected || replacement.first != reduced_lhs) {
        if(reduced_lhs.get_if_conglomerate()) {
          replacement.first = std::move(reduced_lhs); //okay, defining conglomerate
          if(replacement.first == replacement.second) { //should be more general check of dependence rather than equality...
            auto vec_index = &replacement - ret.replacements.data();
            ret.replacements.erase(ret.replacements.begin() + vec_index);
          }
          goto RESTART_REDUCTION;
        } else if(ret.is_axiomatic(reduced_lhs)) {
          if(replacement.second.holds_conglomerate()) {
            //define conglomerate to be this new value
            replacement.first = std::move(replacement.second);
            replacement.second = std::move(reduced_lhs);
            goto RESTART_REDUCTION;
          } else {
            auto unfolded_lhs = expression::unfold(reduced_lhs);
            auto unfolded_rhs = expression::unfold(replacement.second);
            if(unfolded_lhs.head != unfolded_rhs.head || unfolded_lhs.args.size() != unfolded_rhs.args.size()) {
              std::terminate(); //mismatched axioms
            }
            for(std::size_t i = 0; i < unfolded_lhs.args.size(); ++i, ++conglomerate_index) {
              proposed_replacements.emplace_back(
                unfolded_lhs.args[i],
                expression::tree::Conglomerate{conglomerate_index}
              );
              proposed_replacements.emplace_back(
                unfolded_rhs.args[i],
                expression::tree::Conglomerate{conglomerate_index}
              );
            }
            auto vec_index = &replacement - ret.replacements.data();
            ret.replacements.erase(ret.replacements.begin() + vec_index);
            goto RESTART_REDUCTION;
          }
        } else if(ret.is_purely_open(reduced_lhs)) {
          if(!newly_inspected || replacement.first != reduced_lhs) {
            replacement.first = std::move(reduced_lhs);
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
      std::cout << expression::raw_format(replacement.first) << " -> " << expression::raw_format(replacement.second) << "\n";
    }
    return ret;
  }
}
