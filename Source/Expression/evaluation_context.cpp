#include "evaluation_context.hpp"

namespace expression {
  Context::Context() {
    { //Create basic externals
      //0 = Type : Type
      //1 = arrow : arrow Type arrow_codomain
      //2 = type_family = \T:Type.arrow T (constant Type Type T)
      //  : type_family Type
      //3 = constant = \T:Type.\t:T.\S:Type.\s:S.t
      //  : arrow Type constant_codomain_1
      //4 = constant_codomain_1 = \T:Type.arrow T (constant_codomain_2 T)
      //  : type_family Type
      //5 = constant_codomain_2 = \T:Type.\t:T.arrow Type (constant_codomain_3 T)
      //  : arrow Type type_family
      //6 = constant_codomain_3 = \T:Type.\S:Type.arrow S (constant Type T S)
      //  : arrow Type constant_codomain_4
      //7 = constant_codomain_4 = \T:Type.type_family Type
      //  : type_family Type
      //8 = id = \T:Type.\t:T.t
      //  : arrow Type id_codomain
      //9 = id_codomain = \T:Type.arrow T (constant Type T T)
      //  : type_family Type
      //10 = arrow_codomain = \T:Type.arrow (type_family T) (constant Type Type (type_family T))
      //  : type_family Type
      auto type_family_over = [](std::uint64_t ax) { return tree::Expression{tree::Apply{tree::External{2}, tree::External{ax}}}; };
      auto arrow = [](std::uint64_t dom, std::uint64_t codom) { return tree::Expression{tree::Apply{tree::Apply{tree::External{1}, tree::External{dom}}, tree::External{codom}}}; };
      external_info.push_back({ //0 = Type
        .is_axiom = true,
        .type = tree::External{0}
      });
      external_info.push_back({ //1 = arrow
        .is_axiom = true,
        .type = arrow(0, 10)
      });
      external_info.push_back({ //2 = type_family
        .is_axiom = false,
        .type = type_family_over(0)
      });
      external_info.push_back({ //3 = constant
        .is_axiom = false,
        .type = arrow(0, 4)
      });
      external_info.push_back({ //4 = constant_codomain_1
        .is_axiom = false,
        .type = type_family_over(0)
      });
      external_info.push_back({ //5 = constant_codomain_2
        .is_axiom = false,
        .type = arrow(0, 2)
      });
      external_info.push_back({ //6 = constant_codomain_3
        .is_axiom = false,
        .type = arrow(0, 7)
      });
      external_info.push_back({ //7 = constant_codomain_4
        .is_axiom = false,
        .type = type_family_over(0)
      });
      external_info.push_back({ //8 = id
        .is_axiom = false,
        .type = arrow(0, 9)
      });
      external_info.push_back({ //9 = id_codomain
        .is_axiom = false,
        .type = type_family_over(0)
      });
      external_info.push_back({ //10 = arrow_codomain
        .is_axiom = false,
        .type = type_family_over(0)
      });
      add_rule({ //type_family
        .pattern = lambda_pattern(2, 1),
        .replacement = multi_apply(
          tree::External{1},
          tree::Arg{0},
          multi_apply(
            tree::External{3},
            tree::External{0},
            tree::External{0},
            tree::Arg{0}
          )
        )
      });
      add_rule({ //constant
        .pattern = lambda_pattern(3, 4),
        .replacement = tree::Arg{1}
      });
      add_rule({ //constant_codomain_1
        .pattern = lambda_pattern(4, 1),
        .replacement = multi_apply(
          tree::External{1},
          tree::Arg{0},
          tree::Apply{
            tree::External{5},
            tree::Arg{0}
          }
        )
      });
      add_rule({ //constant_codomain_2
        .pattern = lambda_pattern(5, 2),
        .replacement = multi_apply(
          tree::External{1},
          tree::External{0},
          tree::Apply{
            tree::External{6},
            tree::Arg{0}
          }
        )
      });
      add_rule({ //constant_codomain_3
        .pattern = lambda_pattern(6, 2),
        .replacement = multi_apply(
          tree::External{1},
          tree::Arg{1},
          multi_apply(
            tree::External{3},
            tree::External{0},
            tree::Arg{0},
            tree::Arg{1}
          )
        )
      });
      add_rule({ //constant_codomain_4
        .pattern = lambda_pattern(7, 1),
        .replacement = tree::Apply{
          tree::External{2},
          tree::External{0}
        }
      });
      add_rule({ //id
        .pattern = lambda_pattern(8, 2),
        .replacement = tree::Arg{1}
      });
      add_rule({ //id_codomain
        .pattern = lambda_pattern(9, 1),
        .replacement = multi_apply(
          tree::External{1},
          tree::Arg{0},
          multi_apply(
            tree::External{3},
            tree::External{0},
            tree::Arg{0},
            tree::Arg{0}
          )
        )
      });
      add_rule({ //arrow_codomain
        .pattern = lambda_pattern(10, 1),
        .replacement = multi_apply(
          tree::External{1},
          tree::Apply{
            tree::External{2},
            tree::Arg{0}
          },
          multi_apply(
            tree::External{3},
            tree::External{0},
            tree::External{0},
            tree::Apply{
              tree::External{2},
              tree::Arg{0}
            }
          )
        )
      });
    }
    primitives.type = 0;
    primitives.arrow = 1;
    primitives.type_family = 2;
    primitives.constant = 3;
    primitives.constant_codomain_1 = 4;
    primitives.constant_codomain_2 = 5;
    primitives.constant_codomain_3 = 6;
    primitives.constant_codomain_4 = 7;
    primitives.id = 8;
    primitives.id_codomain = 9;
    primitives.arrow_codomain = 10;
  }
  namespace {
    template<class Filter>
    tree::Expression reduce_impl(Context& ctx, tree::Expression tree, Filter&& filter) {
    REDUCTION_START:
      auto unfolded = unfold_ref(tree);
      for(auto* arg : unfolded.args) {
        *arg = reduce_impl(ctx, std::move(*arg), filter);
      }
      if(auto* ext = unfolded.head->get_if_external()) {
        auto const& ext_info = ctx.external_info[ext->external_index];
        for(auto const& rule_info : ext_info.rules) {
          if(rule_info.arg_count <= unfolded.args.size()) {
            auto test_pos = unfolded.spine.at(rule_info.arg_count);
            auto const& rule = ctx.rules.at(rule_info.index);
            if(!filter(rule)) continue;
            if(term_matches(*test_pos, rule.pattern)) {
              replace_with_substitution_at(test_pos, rule.pattern, rule.replacement);
              goto REDUCTION_START;
            }
          }
        }
        for(auto const& rule_info : ext_info.data_rules) {
          if(rule_info.arg_count <= unfolded.args.size()) {
            auto test_pos = unfolded.spine.at(rule_info.arg_count);
            auto const& rule = ctx.data_rules.at(rule_info.index);
            if(term_matches(*test_pos, rule.pattern)) {
              *test_pos = rule.replace(destructure_match(std::move(*test_pos), rule.pattern));
              goto REDUCTION_START;
            }
          }
        }
      }
      return std::move(tree);
    }
  }
  tree::Expression Context::reduce(tree::Expression tree) {
    return reduce_impl(*this, std::move(tree), [](auto&&) { return true; });
  }
  tree::Expression Context::reduce_filter_rules(tree::Expression tree, mdb::function<bool(Rule const&)> filter) {
    return reduce_impl(*this, std::move(tree), std::move(filter));
  }
  TypedValue Context::get_external(std::uint64_t i) {
    return {
      .value = tree::External{i},
      .type = external_info[i].type
    };
  }
  void Context::add_rule(Rule rule) {
    auto index = rules.size();
    auto head = get_pattern_head(rule.pattern);
    auto args = count_pattern_args(rule.pattern);
    rules.push_back(std::move(rule));
    external_info[head].rules.push_back({
      .index = index,
      .arg_count = args
    });
  }
  void Context::add_data_rule(DataRule rule) {
    auto index = data_rules.size();
    auto head = get_pattern_head(rule.pattern);
    auto args = count_pattern_args(rule.pattern);
    data_rules.push_back(std::move(rule));
    external_info[head].data_rules.push_back({
      .index = index,
      .arg_count = args
    });
  }

  std::optional<Context::FunctionData> Context::get_domain_and_codomain(tree::Expression in) {
    in = reduce(std::move(in));
    if(auto* lhs_apply = in.get_if_apply()) {
      if(auto* inner_apply = lhs_apply->lhs.get_if_apply()) {
        if(auto* lhs_ext = inner_apply->lhs.get_if_external()) {
          if(lhs_ext->external_index == primitives.arrow) {
            return FunctionData{
              .domain = std::move(inner_apply->rhs),
              .codomain = std::move(lhs_apply->rhs)
            };
          }
        }
      }
    }
    return std::nullopt;
  }
  tree::Expression Unfolding::fold() && {
    for(auto& arg : args) {
      head = tree::Apply{
        std::move(head),
        std::move(arg)
      };
    }
    return std::move(head);
  }
}
