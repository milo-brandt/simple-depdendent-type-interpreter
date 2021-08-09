#include "expression_tree.hpp"
#include <algorithm>

namespace expression {
  bool term_matches(tree::Tree const& term, pattern::Tree const& pattern) {
    return pattern.visit(mdb::overloaded{
      [&](pattern::Apply const& apply) {
        if(auto* app = term.get_if_apply()) {
          return term_matches(app->lhs, apply.lhs) && term_matches(app->rhs, apply.rhs);
        } else {
          return false;
        }
      },
      [&](pattern::Fixed const& fixed) {
        if(auto* ext = term.get_if_external()) {
          return ext->index == fixed.index;
        } else {
          return false;
        }
      },
      [&](pattern::Wildcard const&) {
        return true;
      }
    });
  }
  namespace {
    template<class T, class Callback>
    void destructure_match_impl(T term, pattern::Tree const& pattern, Callback&& insert) {
      pattern.visit(mdb::overloaded{
        [&](pattern::Apply const& apply) {
          if(auto* app = term.get_if_apply()) {
            destructure_match_impl<T>(app->lhs, apply.lhs, insert);
            destructure_match_impl<T>(app->rhs, apply.rhs, insert);
          } else {
            throw NoMatchException{};
          }
        },
        [&](pattern::Fixed const& fixed) {},
        [&](pattern::Wildcard const&) {
          insert(term);
        }
      });
    }
  }
  std::vector<tree::Tree*> destructure_match_ref(tree::Tree& term, pattern::Tree const& pattern) {
    std::vector<tree::Tree*> ret;
    destructure_match_impl<tree::Tree&>(term, pattern, [&](tree::Tree& term) { ret.push_back(&term); });
    return ret;
  }
  std::vector<tree::Tree const*> destructure_match_ref(tree::Tree const& term, pattern::Tree const& pattern) {
    std::vector<tree::Tree const*> ret;
    destructure_match_impl<tree::Tree const&>(term, pattern, [&](tree::Tree const& term) { ret.push_back(&term); });
    return ret;
  }
  std::vector<tree::Tree> destructure_match(tree::Tree term, pattern::Tree const& pattern) {
    std::vector<tree::Tree> ret;
    destructure_match_impl<tree::Tree&>(term, pattern, [&](tree::Tree& term) { ret.push_back(std::move(term)); });
    return ret;
  }
  std::vector<path::Path> captures_of_pattern(pattern::Tree const& pattern) {
    struct Impl {
      std::vector<path::Path> ret;
      path::Path path;
      void collect(pattern::Tree const& pattern) {
        pattern.visit(mdb::overloaded{
          [&](pattern::Apply const& apply) {
            path_append(path, &tree::Apply::lhs);
            collect(apply.lhs);
            path.steps.pop_back();
            path_append(path, &tree::Apply::rhs);
            collect(apply.rhs);
            path.steps.pop_back();
          },
          [&](pattern::Fixed const& fixed) {},
          [&](pattern::Wildcard const&) {
            ret.push_back(path);
          }
        });
      }
    };
    Impl impl{};
    impl.collect(pattern);
    return std::move(impl.ret);
  }
  std::vector<path::Path> find_all_matches(tree::Tree const& term, pattern::Tree const& pattern) {
    std::vector<path::Path> ret;
    for(auto position : recursive_range(term)) {
      if(term_matches(position.value, pattern)) {
        ret.push_back(position.path);
      }
    }
    return ret;
  }
  tree::Tree substitute_into_replacement(std::vector<tree::Tree> const& terms, tree::Tree const& replacement) {
    return replacement.visit(mdb::overloaded{
      [&](tree::Apply const& apply) -> tree::Tree {
        return tree::Apply{
          .lhs = substitute_into_replacement(terms, apply.lhs),
          .rhs = substitute_into_replacement(terms, apply.rhs)
        };
      },
      [&](tree::External const& external) -> tree::Tree {
        return external; //copy
      },
      [&](tree::Arg const& arg) -> tree::Tree {
        if(arg.index < terms.size()) {
          return terms[arg.index]; //copy
        } else {
          throw NotEnoughArguments{};
        }
      }
    });
  }
  void replace_with_substitution_at(tree::Tree& term, path::Path const& path, pattern::Tree const& pattern, tree::Tree const& replacement) {
    tree::Tree& inner_term = path_lookup(term, path);
    auto captures = destructure_match(std::move(inner_term), pattern);
    inner_term = substitute_into_replacement(captures, replacement);
  }

  RefUnfolding unfold_ref(tree::Tree const& tree) {
    RefUnfolding ret{
      .head = &tree
    };
    while(auto* app = ret.head->get_if_apply()) {
      ret.head = &app->lhs;
      ret.args.push_back(&app->rhs);
    }
    std::reverse(ret.args.begin(), ret.args.end());
    return ret;
  }
  Unfolding unfold(tree::Tree tree) {
    Unfolding ret{
      .head = std::move(tree)
    };
    while(auto* app = ret.head.get_if_apply()) {
      ret.args.push_back(std::move(app->rhs));
      ret.head = std::move(app->lhs);
    }
    std::reverse(ret.args.begin(), ret.args.end());
    return ret;

  }
}
