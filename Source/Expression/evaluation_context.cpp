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
    tree::Expression reduce_flat(Context& ctx, tree::Expression tree, Filter&& filter) {
      struct TreeHasher {
        std::hash<void const*> h;
        auto operator()(tree::Expression const& expr) const noexcept {
          return h(expr.data());
        }
      };
      struct StackFrame {
        tree::Expression head;
        std::uint64_t arg_count;
        std::uint64_t next_arg_to_reduce;
        std::size_t result_position;
      };
      std::unordered_map<tree::Expression, tree::Expression, TreeHasher> memoized_results;
      std::vector<tree::Expression> arg_stack;
      std::vector<StackFrame> stack;
      auto push_stack_frame = [&](std::size_t result_position) {
        //Push the expressions spine and args to the appropriate stacks, and
        //push a stack frame with the requisite information.
        auto head = arg_stack[result_position];
        auto next_arg_to_reduce = arg_stack.size();
        std::uint64_t arg_count = 0;
        while(auto* apply = head.get_if_apply()) {
          arg_stack.push_back(apply->rhs);
          head = apply->lhs;
          ++arg_count;
        }
        stack.push_back({
          .head = std::move(head),
          .arg_count = arg_count,
          .next_arg_to_reduce = next_arg_to_reduce,
          .result_position = result_position
        });
      };
      auto reduce_next_arg = [&] { //returns "true" if current frame is fully reduced
        auto& stack_top = stack.back();
        std::size_t stack_top_index = stack.size() - 1;
        while(stack_top.next_arg_to_reduce < arg_stack.size()) {
          if(memoized_results.contains(arg_stack[stack_top.next_arg_to_reduce])) {
            arg_stack[stack_top.next_arg_to_reduce] = memoized_results.at(arg_stack[stack_top.next_arg_to_reduce]);
            ++stack[stack_top_index].next_arg_to_reduce;
          } else {
            push_stack_frame(stack_top.next_arg_to_reduce);
            ++stack[stack_top_index].next_arg_to_reduce;
            return true;
          }
        }
        return false;
      };
      auto recombine_head = [&](tree::Expression head, std::size_t arg_count) {
        for(std::size_t i = 0; i < arg_count; ++i) {
          head = tree::Apply{
            std::move(head),
            arg_stack[arg_stack.size() - i - 1]
          };
        }
        return head;
      };
      auto find_local_reduction = [&] { //returns "true" if something was changed; head should be new head, args popped from stack if used
        auto& stack_top = stack.back();
        auto& head = stack_top.head;
        if(auto* ext = head.get_if_external()) {
          auto const& ext_info = ctx.external_info[ext->external_index];
          for(auto const& rule_info : ext_info.rules) {
            if(rule_info.arg_count <= stack_top.arg_count) {
              auto test_pos = recombine_head(head, rule_info.arg_count);
              auto const& rule = ctx.rules[rule_info.index];
              if(!filter(rule)) continue;
              if(term_matches(test_pos, rule.pattern)) {
                head = substitute_into_replacement(destructure_match(test_pos, rule.pattern), rule.replacement);
                arg_stack.erase(arg_stack.end() - rule_info.arg_count, arg_stack.end());
                stack_top.arg_count -= rule_info.arg_count;
                return true;
              }
            }
          }
          for(auto const& rule_info : ext_info.data_rules) {
            if(rule_info.arg_count <= stack_top.arg_count) {
              auto test_pos = recombine_head(head, rule_info.arg_count);
              auto const& rule = ctx.data_rules[rule_info.index];
              if(term_matches(test_pos, rule.pattern)) {
                head = rule.replace(destructure_match(test_pos, rule.pattern));
                arg_stack.erase(arg_stack.end() - rule_info.arg_count, arg_stack.end());
                stack_top.arg_count -= rule_info.arg_count;
                return true;
              }
            }
          }
        }
        return false;
      };
      auto pop_stack_frame = [&](bool save_result) {
        auto& stack_top = stack.back();
        auto root_head = recombine_head(stack_top.head, stack_top.arg_count);
        arg_stack.erase(arg_stack.end() - stack_top.arg_count, arg_stack.end());
        if(save_result) {
          memoized_results.insert(std::make_pair(arg_stack[stack_top.result_position], root_head));
        }
        arg_stack[stack_top.result_position] = std::move(root_head);
        stack.pop_back();
      };
      auto repush_top_frame = [&] { //pop and push a frame after simplifying
        auto frame_pos = stack.back().result_position;
        pop_stack_frame(false);
        push_stack_frame(frame_pos);
      };
      /*
        Body
      */
      arg_stack.push_back(tree);
      push_stack_frame(0);
      while(!stack.empty()) {
        while(reduce_next_arg()); //push args onto stack as long as we can
        //At this point, the top stack frame has reduced all of its args.
        if(find_local_reduction()) {
          repush_top_frame();
        } else {
          pop_stack_frame(true);
        }
      }
      return std::move(arg_stack[0]);
    }
    /*
    template<class Filter>
    tree::Expression reduce_impl(Context& ctx, tree::Expression tree, Filter&& filter) {
      struct Detail {
        Context& ctx;
        Filter& filter;
        std::vector<tree::Expression*> spine_stack;
        std::vector<tree::Expression*> arg_stack;
        void reduce(tree::Expression* expr) {
        REDUCTION_START:
          //1. Unfold expr
          std::uint64_t base_arg_index = arg_stack.size();
          std::uint64_t arg_count = 0;
          tree::Expression* head = expr;
          spine_stack.push_back(head);
          while(auto* apply = head->get_if_apply()) {
            arg_stack.push_back(&apply->rhs);
            head = &apply->lhs;
            spine_stack.push_back(head);
            ++arg_count;
          }
          //Top of stacks looks like
          //spine_stack: spine@3 spine@2 spine@1 spine@0
          //arg_stack: arg2 arg1 arg0
          std::uint64_t spine_back_index = spine_stack.size() - 1;
          for(std::uint64_t i = base_arg_index; i < base_arg_index + arg_count; ++i) {
            reduce(arg_stack[i]);
          }
          if(auto* ext = head->get_if_external()) {
            auto const& ext_info = ctx.external_info[ext->external_index];
            for(auto const& rule_info : ext_info.rules) {
              if(rule_info.arg_count <= arg_count) {
                auto test_pos = spine_stack[spine_back_index - rule_info.arg_count];
                auto const& rule = ctx.rules[rule_info.index];
                if(!filter(rule)) continue;
                if(term_matches(*test_pos, rule.pattern)) {
                  replace_with_substitution_at(test_pos, rule.pattern, rule.replacement);
                  goto REDUCTION_START;
                }
              }
            }
            for(auto const& rule_info : ext_info.data_rules) {
              if(rule_info.arg_count <= arg_count) {
                auto test_pos = spine_stack[spine_back_index - rule_info.arg_count];
                auto const& rule = ctx.data_rules[rule_info.index];
                if(term_matches(*test_pos, rule.pattern)) {
                  *test_pos = rule.replace(destructure_match(std::move(*test_pos), rule.pattern));
                  goto REDUCTION_START;
                }
              }
            }
          }
        }
      };
      Detail{ctx, filter}.reduce(&tree);
      return std::move(tree);
    }*/
  }
  /*
    Patterns can...
      1. request a subpattern be matched against an axiomatic expression...
      2. re-use results of these matches
  */
  tree::Expression Context::reduce(tree::Expression tree) {
    return reduce_flat(*this, std::move(tree), [](auto&&) { return true; });
  }
  tree::Expression Context::reduce_filter_rules(tree::Expression tree, mdb::function<bool(Rule const&)> filter) {
    return reduce_flat(*this, std::move(tree), std::move(filter));
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
  void Context::replace_rule(std::size_t index, Rule new_rule) {
    auto head = get_pattern_head(new_rule.pattern);
    auto args = count_pattern_args(new_rule.pattern);
    for(auto& rule_info : external_info[head].rules) {
      if(rule_info.index == index) {
        rules[index] = std::move(new_rule);
        rule_info.arg_count = args;
        return;
      }
    }
    std::terminate(); //o no
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
