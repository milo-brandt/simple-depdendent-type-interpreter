#include "expression_tree.hpp"
#include <algorithm>

namespace expression {
  bool term_matches(tree::Expression const& term, pattern::Pattern const& pattern) {
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
          return ext->external_index == fixed.external_index;
        } else {
          return false;
        }
      },
      [&](pattern::Wildcard const&) {
        return true;
      }
    });
  }
  bool term_matches(tree::Expression const& term, data_pattern::Pattern const& pattern) {
    return pattern.visit(mdb::overloaded{
      [&](data_pattern::Apply const& apply) {
        if(auto* app = term.get_if_apply()) {
          return term_matches(app->lhs, apply.lhs) && term_matches(app->rhs, apply.rhs);
        } else {
          return false;
        }
      },
      [&](data_pattern::Fixed const& fixed) {
        if(auto* ext = term.get_if_external()) {
          return ext->external_index == fixed.external_index;
        } else {
          return false;
        }
      },
      [&](data_pattern::Wildcard const&) {
        return true;
      },
      [&](data_pattern::Data const& data) {
        if(auto* dat = term.get_if_data()) {
          return dat->data.get_type_index() == data.type_index;
        } else {
          return false;
        }
      }
    });
  }

  namespace {
    tree::Expression trivial_replacement_for_impl(pattern::Pattern const& pat, std::uint64_t& index) {
      return pat.visit(mdb::overloaded{
        [&](pattern::Apply const& apply) -> tree::Expression {
            auto lhs = trivial_replacement_for_impl(apply.lhs, index);
            auto rhs = trivial_replacement_for_impl(apply.rhs, index);
            return tree::Apply{
              .lhs = std::move(lhs),
              .rhs = std::move(rhs)
            };
        },
        [&](pattern::Fixed const& fixed) -> tree::Expression {
          return tree::External{fixed.external_index};
        },
        [&](pattern::Wildcard const&) -> tree::Expression {
          return tree::Arg{index++};
        }
      });
    }
  }
  tree::Expression trivial_replacement_for(pattern::Pattern const& pat) { //mostly for printing
    std::uint64_t index = 0;
    return trivial_replacement_for_impl(pat, index);
  }
  namespace {
    template<class T, class Callback>
    void destructure_match_impl(T term, pattern::Pattern const& pattern, Callback&& insert) {
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
  std::vector<tree::Expression*> destructure_match_ref(tree::Expression& term, pattern::Pattern const& pattern) {
    std::vector<tree::Expression*> ret;
    destructure_match_impl<tree::Expression&>(term, pattern, [&](tree::Expression& term) { ret.push_back(&term); });
    return ret;
  }
  std::vector<tree::Expression const*> destructure_match_ref(tree::Expression const& term, pattern::Pattern const& pattern) {
    std::vector<tree::Expression const*> ret;
    destructure_match_impl<tree::Expression const&>(term, pattern, [&](tree::Expression const& term) { ret.push_back(&term); });
    return ret;
  }
  std::vector<tree::Expression> destructure_match(tree::Expression term, pattern::Pattern const& pattern) {
    std::vector<tree::Expression> ret;
    destructure_match_impl<tree::Expression&>(term, pattern, [&](tree::Expression& term) { ret.push_back(std::move(term)); });
    return ret;
  }
  namespace {
    template<class T, class Callback>
    void destructure_match_impl(T term, data_pattern::Pattern const& pattern, Callback&& insert) {
      pattern.visit(mdb::overloaded{
        [&](data_pattern::Apply const& apply) {
          if(auto* app = term.get_if_apply()) {
            destructure_match_impl<T>(app->lhs, apply.lhs, insert);
            destructure_match_impl<T>(app->rhs, apply.rhs, insert);
          } else {
            throw NoMatchException{};
          }
        },
        [&](data_pattern::Fixed const& fixed) {},
        [&](data_pattern::Wildcard const&) {
          insert(term);
        },
        [&](data_pattern::Data const& data) {
          insert(term);
        }
      });
    }
  }
  std::vector<tree::Expression> destructure_match(tree::Expression term, data_pattern::Pattern const& pattern) {
    std::vector<tree::Expression> ret;
    destructure_match_impl<tree::Expression&>(term, pattern, [&](tree::Expression& term) { ret.push_back(std::move(term)); });
    return ret;
  }

  std::vector<tree::Expression*> find_all_matches(tree::Expression& term, pattern::Pattern const& pattern) {
    struct Detail {
      std::vector<tree::Expression*> ret;
      pattern::Pattern const& pattern;
      void search(tree::Expression& expression) {
        if(term_matches(expression, pattern)) {
          ret.push_back(&expression);
        }
        if(auto* apply = expression.get_if_apply()) {
          search(apply->lhs);
          search(apply->rhs);
        }
      }
    };
    Detail detail{.pattern = pattern};
    detail.search(term);
    return std::move(detail.ret);
  }
  std::vector<tree::Expression const*> find_all_matches(tree::Expression const& term, pattern::Pattern const& pattern) {
    struct Detail {
      std::vector<tree::Expression const*> ret;
      pattern::Pattern const& pattern;
      void search(tree::Expression const& expression) {
        if(term_matches(expression, pattern)) {
          ret.push_back(&expression);
        }
        if(auto* apply = expression.get_if_apply()) {
          search(apply->lhs);
          search(apply->rhs);
        }
      }
    };
    Detail detail{.pattern = pattern};
    detail.search(term);
    return std::move(detail.ret);
  }
  std::vector<tree::Expression*> find_all_matches(tree::Expression& term, data_pattern::Pattern const& pattern) {
    struct Detail {
      std::vector<tree::Expression*> ret;
      data_pattern::Pattern const& pattern;
      void search(tree::Expression& expression) {
        if(term_matches(expression, pattern)) {
          ret.push_back(&expression);
        }
        if(auto* apply = expression.get_if_apply()) {
          search(apply->lhs);
          search(apply->rhs);
        }
      }
    };
    Detail detail{.pattern = pattern};
    detail.search(term);
    return std::move(detail.ret);
  }
  std::vector<tree::Expression const*> find_all_matches(tree::Expression const& term, data_pattern::Pattern const& pattern) {
    struct Detail {
      std::vector<tree::Expression const*> ret;
      data_pattern::Pattern const& pattern;
      void search(tree::Expression const& expression) {
        if(term_matches(expression, pattern)) {
          ret.push_back(&expression);
        }
        if(auto* apply = expression.get_if_apply()) {
          search(apply->lhs);
          search(apply->rhs);
        }
      }
    };
    Detail detail{.pattern = pattern};
    detail.search(term);
    return std::move(detail.ret);
  }

  tree::Expression substitute_into_replacement(std::vector<tree::Expression> const& terms, tree::Expression const& replacement) {
    return replacement.visit(mdb::overloaded{
      [&](tree::Apply const& apply) -> tree::Expression {
        return tree::Apply{
          .lhs = substitute_into_replacement(terms, apply.lhs),
          .rhs = substitute_into_replacement(terms, apply.rhs)
        };
      },
      [&](tree::External const& external) -> tree::Expression {
        return external; //copy
      },
      [&](tree::Arg const& arg) -> tree::Expression {
        if(arg.arg_index < terms.size()) {
          return terms[arg.arg_index]; //copy
        } else {
          throw NotEnoughArguments{};
        }
      },
      [&](tree::Data const& data) -> tree::Expression {
        return data.data.substitute(terms);
      }
    });
  }
  std::optional<tree::Expression> remap_args(std::unordered_map<std::uint64_t, std::uint64_t> const& arg_map, tree::Expression const& target) {
    struct Detail {
      std::unordered_map<std::uint64_t, std::uint64_t> const& arg_map;
      std::uint64_t arg_count = 0;
      bool is_acceptable(tree::Expression const& expr) {
        return expr.visit(mdb::overloaded{
          [&](tree::Apply const& apply) {
            return is_acceptable(apply.lhs) && is_acceptable(apply.rhs);
          },
          [&](tree::External const& external) {
            return true;
          },
          [&](tree::Arg const& arg) {
            if(arg.arg_index >= arg_count) arg_count = arg.arg_index + 1;
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
      std::vector<tree::Expression> get_remap() {
        std::vector<tree::Expression> ret;
        ret.reserve(arg_count);
        for(std::uint64_t i = 0; i < arg_count; ++i) {
          if(arg_map.contains(i)) {
            ret.push_back(tree::Arg{arg_map.at(i)});
          } else {
            ret.push_back(tree::Arg{(std::uint64_t)-1});
          }
        }
        return ret;
      }
    };
    Detail detail{arg_map};
    if(!detail.is_acceptable(target)) return std::nullopt;
    return substitute_into_replacement(detail.get_remap(), target);
  }
  void replace_with_substitution_at(tree::Expression* term, pattern::Pattern const& pattern, tree::Expression const& replacement) {
    auto captures = destructure_match(std::move(*term), pattern);
    *term = substitute_into_replacement(captures, replacement);
  }
  std::uint64_t get_pattern_head(expression::pattern::Pattern const& pat) {
    if(auto* apply = pat.get_if_apply()) {
      return get_pattern_head(apply->lhs);
    } else {
      return pat.get_fixed().external_index;
    }
  }

  RefUnfolding unfold_ref(tree::Expression const& tree) {
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
  Unfolding unfold(tree::Expression tree) {
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
