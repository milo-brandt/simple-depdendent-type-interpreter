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
      rules.push_back({ //type_family
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
      rules.push_back({ //constant
        .pattern = lambda_pattern(3, 4),
        .replacement = tree::Arg{1}
      });
      rules.push_back({ //constant_codomain_1
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
      rules.push_back({ //constant_codomain_2
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
      rules.push_back({ //constant_codomain_3
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
      rules.push_back({ //constant_codomain_4
        .pattern = lambda_pattern(7, 1),
        .replacement = tree::Apply{
          tree::External{2},
          tree::External{0}
        }
      });
      rules.push_back({ //id
        .pattern = lambda_pattern(8, 2),
        .replacement = tree::Arg{1}
      });
      rules.push_back({ //id_codomain
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
      rules.push_back({ //arrow_codomain
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
  tree::Expression Context::reduce(tree::Expression tree) {
  REDUCTION_START:
    for(auto const& rule : rules) {
      auto vec = find_all_matches(tree, rule.pattern);
      if(!vec.empty()) {
        replace_with_substitution_at(vec[0], rule.pattern, rule.replacement);
        goto REDUCTION_START;
      }
    }
    return tree;
  }
  tree::Expression Context::reduce_filer_rules(tree::Expression tree, mdb::function<bool(Rule const&)> filter) {
    ROOT_REDUCTION_START:
    for(auto const& rule : rules) {
      if(!filter(rule)) continue;
      auto vec = find_all_matches(tree, rule.pattern);
      if(!vec.empty()) {
        replace_with_substitution_at(vec[0], rule.pattern, rule.replacement);
        goto ROOT_REDUCTION_START;
      }
    }
    return tree;
  }
  TypedValue Context::get_external(std::uint64_t i) {
    return {
      .value = tree::External{i},
      .type = external_info[i].type
    };
  }
}
