#include "rule_utility.hpp"
#include "../NewExpression/arena_utility.hpp"
#include "../Utility/vector_utility.hpp"

namespace solver {
  namespace instruction_archive = compiler::new_instruction::output::archive_part;
  pattern_expr::PatternExpr resolve_pattern(compiler::new_instruction::output::archive_part::Pattern const& pattern, PatternResolveInterface const& interface) {
    return pattern.visit(mdb::overloaded{
      [&](instruction_archive::PatternApply const& apply) -> pattern_expr::PatternExpr {
        return pattern_expr::Apply{
          .lhs = resolve_pattern(apply.lhs, interface),
          .rhs = resolve_pattern(apply.rhs, interface)
        };
      },
      [&](instruction_archive::PatternLocal const& local) -> pattern_expr::PatternExpr {
        return pattern_expr::Embed{
          interface.lookup_local(local.local_index)
        };
      },
      [&](instruction_archive::PatternEmbed const& embed) -> pattern_expr::PatternExpr {
        return pattern_expr::Embed{
          interface.lookup_embed(embed.embed_index)
        };
      },
      [&](instruction_archive::PatternCapture const& capture) -> pattern_expr::PatternExpr {
        return pattern_expr::Capture {
          .capture_index = capture.capture_index
        };
      },
      [&](instruction_archive::PatternHole const&) -> pattern_expr::PatternExpr {
        return pattern_expr::Hole{};
      }
    });
  }
  namespace {
    struct PatternExprUnfolding {
      pattern_expr::PatternExpr* head;
      std::vector<pattern_expr::PatternExpr*> args;
    };
    PatternExprUnfolding unfold(pattern_expr::PatternExpr& expr) {
      PatternExprUnfolding ret{
        .head = &expr
      };
      while(auto apply = ret.head->get_if_apply()) {
        ret.head = &apply->lhs;
        ret.args.push_back(&apply->rhs);
      }
      std::reverse(ret.args.begin(), ret.args.end());
      return ret;
    }
    pattern_node::PatternNode convert_to_node(new_expression::Arena& arena, pattern_expr::PatternExpr expr) {
      auto pattern_unfolded = unfold(expr);
      auto get_check = [&]() -> pattern_node::PatternNode {
        return pattern_node::Check{
          .desired_equality = std::move(expr)
        };
      };
      return pattern_unfolded.head->visit(mdb::overloaded{
        [&](pattern_expr::Apply& apply) -> pattern_node::PatternNode {
          std::terminate(); //unreachable
        },
        [&](pattern_expr::Embed& embed) -> pattern_node::PatternNode {
          auto unfolded = unfold(arena, embed.value);
          if(arena.holds_axiom(unfolded.head)) {
            std::vector<pattern_node::PatternNode> pattern_nodes;
            for(auto& arg : unfolded.args) {
              pattern_nodes.push_back(pattern_node::Check{
                pattern_expr::Embed{
                  arena.copy(arg)
                }
              });
            }
            for(auto* pat_arg : pattern_unfolded.args) {
              pattern_nodes.push_back(convert_to_node(arena, std::move(*pat_arg)));
            }
            auto head = arena.copy(unfolded.head);
            arena.drop(std::move(embed.value));
            return pattern_node::Apply{
              .args = std::move(pattern_nodes),
              .head = std::move(head)
            };
          } else {
            return get_check();
          }
        },
        [&](pattern_expr::Capture& capture) -> pattern_node::PatternNode {
          if(pattern_unfolded.args.size() > 0) {
            return get_check();
          } else {
            return pattern_node::Capture{
              .capture_index = capture.capture_index
            };
          }
        },
        [&](pattern_expr::Hole& hole) -> pattern_node::PatternNode {
          if(pattern_unfolded.args.size() > 0) {
            return get_check();
          } else {
            return pattern_node::Ignore{};
          }
        }
      });
    }
  }
  FoldedPattern normalize_pattern(new_expression::Arena& arena, RawPattern raw, std::size_t capture_count) {
    //step 1: figure out how many arguments are on head.
    auto head_pattern_unfolding = unfold(raw.primary_pattern);
    auto& head_value = head_pattern_unfolding.head->get_embed();
    auto head_value_unfolding = unfold(arena, head_value.value);
    if(!arena.holds_declaration(head_value_unfolding.head)) std::terminate();
    for(std::size_t i = 0; i < head_value_unfolding.args.size(); ++i) {
      if(auto* arg = arena.get_if_argument(head_value_unfolding.args[i])) {
        if(arg->index == i) {
          //yay, don't crash
        } else {
          std::terminate();
        }
      } else {
        std::terminate();
      }
    }
    std::size_t arg_count = head_value_unfolding.args.size();
    new_expression::OwnedExpression head = arena.copy(head_value_unfolding.head);
    arena.drop(std::move(head_value.value));
    auto matches = mdb::map([&](pattern_expr::PatternExpr* expr) {
      return convert_to_node(arena, std::move(*expr));
    }, head_pattern_unfolding.args);
    auto subclause_matches = mdb::map([&](RawPatternShard&& shard) {
      return FoldedSubclause{
        .used_captures = std::move(shard.used_captures),
        .node = convert_to_node(arena, std::move(shard.pattern)).get_apply()
      };
    }, std::move(raw.subpatterns));
    return FoldedPattern{
      .head = std::move(head),
      .stack_arg_count = arg_count,
      .capture_count = capture_count,
      .matches = std::move(matches),
      .subclause_matches = std::move(subclause_matches)
    };
  }
  FlatPattern flatten_pattern(new_expression::Arena& arena, FoldedPattern folded) {
    struct Detail {
      new_expression::Arena& arena;
      std::vector<std::optional<std::size_t> > capture_positions;
      std::size_t next_arg;
      std::vector<FlatPatternShard> shards;
      std::vector<FlatPatternCheck> checks;
      bool has_needed_captures(std::vector<std::uint64_t> const& requested) {
        for(auto& request : requested) {
          if(!capture_positions[request]) {
            return false;
          }
        }
        return true;
      }
      std::vector<new_expression::OwnedExpression> get_captures_for(std::vector<std::uint64_t> const& requested) {
        return mdb::map([&](std::uint64_t request) {
          auto& position = capture_positions[request];
          if(!position) std::terminate();
          return arena.argument(*position);
        }, requested);
      }
      void process(pattern_node::PatternNode node, FlatPatternMatchExpr subexpr) {
        node.visit(mdb::overloaded{
          [&](pattern_node::Apply& apply) {
            shards.push_back(FlatPatternMatch{
              .matched_expr = std::move(subexpr),
              .match_head = std::move(apply.head),
              .capture_count = apply.args.size()
            });
            auto arg_pos = next_arg;
            next_arg += apply.args.size();
            for(auto& arg : apply.args) {
              process(std::move(arg), FlatPatternMatchArg{arg_pos++});
            }
          },
          [&](pattern_node::Capture& capture) {
            //this function will only be called for MatchArgs, never Subexpressions
            auto index = std::get<0>(subexpr).matched_arg_index;
            if(capture_positions[capture.capture_index]) {
              checks.push_back({
                .arg_index = index,
                .expected_value = pattern_expr::Capture{
                  capture.capture_index
                }
              });
            } else {
              capture_positions[capture.capture_index] = index;
            }
          },
          [&](pattern_node::Check& check) {
            auto index = std::get<0>(subexpr).matched_arg_index;
            checks.push_back({
              .arg_index = index,
              .expected_value = std::move(check.desired_equality)
            });
          },
          [&](pattern_node::Ignore&) {
            //nothing to do
          }
        });
      }
      std::vector<new_expression::OwnedExpression> get_captures() {
        return mdb::map([&](std::optional<std::size_t> const& position) {
          if(!position) std::terminate();
          return arena.argument(*position);
        }, capture_positions);
      }
    };
    auto arg_count = folded.stack_arg_count + folded.matches.size();
    Detail detail{
      .arena = arena,
      .capture_positions = std::vector<std::optional<std::size_t> >(folded.capture_count, std::optional<std::size_t>{}),
      .next_arg = folded.stack_arg_count
    };
    std::size_t args_pulled = 0;
    auto pull_argument = [&] {
      detail.shards.push_back(FlatPatternPullArgument{});
      detail.process(
        std::move(folded.matches[args_pulled++]),
        FlatPatternMatchArg{detail.next_arg++}
      );
    };
    auto can_pull_argument = [&] {
      return args_pulled != folded.matches.size();
    };
    std::size_t sub_index = 0;
    for(auto& sub : folded.subclause_matches) {
    TRY_TO_INSTANTIATE_SUBCLAUSE:
      if(detail.has_needed_captures(sub.used_captures)) {
        auto head = FlatPatternMatchSubexpression{
          .requested_captures = detail.get_captures_for(sub.used_captures),
          .matched_subexpression = sub_index++
        };
        detail.process(
          std::move(sub.node),
          std::move(head)
        );
      } else {
        if(can_pull_argument()) {
          pull_argument();
          goto TRY_TO_INSTANTIATE_SUBCLAUSE;
        } else {
          std::terminate(); //couldn't capture first thing
        }
      }
    }
    while(can_pull_argument()) pull_argument();
    return FlatPattern {
      .head = std::move(folded.head),
      .stack_arg_count = folded.stack_arg_count,
      .arg_count = arg_count,
      .shards = std::move(detail.shards),
      .captures = detail.get_captures(),
      .checks = std::move(detail.checks)
    };
  }
  using Stack = stack::Stack;
  namespace {
    new_expression::OwnedExpression as_expression(new_expression::Arena& arena, pattern_expr::PatternExpr expr, std::vector<new_expression::OwnedExpression> const& captures) {
      return expr.visit(mdb::overloaded{
        [&](pattern_expr::Apply& apply) {
          return arena.apply(
            as_expression(arena, std::move(apply.lhs), captures),
            as_expression(arena, std::move(apply.rhs), captures)
          );
        },
        [&](pattern_expr::Embed& embed) {
          return std::move(embed.value);
        },
        [&](pattern_expr::Capture& capture) {
          return arena.copy(captures.at(capture.capture_index));
        },
        [&](pattern_expr::Hole&) -> new_expression::OwnedExpression {
          std::terminate(); //can't yet deal with holes appearing!
        }
      });
    }
    new_expression::OwnedExpression extract_application_domain(new_expression::Arena& arena, Stack& stack, new_expression::WeakExpression arrow, new_expression::OwnedExpression& fn, new_expression::OwnedExpression arg) {
      auto type_of = stack.reduce(stack.type_of(fn));
      if(auto fn_info = get_function_data(arena, type_of, arrow)) {
        fn = arena.apply(
          std::move(fn),
          std::move(arg)
        );
        auto ret = arena.copy(fn_info->domain);
        arena.drop(std::move(type_of));
        return ret;
      } else {
        std::terminate();
      }
    }
    Stack extend_by_assumption_typeless(Stack stack, new_expression::OwnedExpression lhs, new_expression::OwnedExpression rhs) {
      return stack.extend_by_assumption(new_expression::TypedValue{
        .value = std::move(lhs),
        .type = stack.type_of(lhs)
      }, new_expression::TypedValue{
        .value = std::move(rhs),
        .type = stack.type_of(rhs)
      });
    }
  }
  PatternExecutionResult execute_pattern(PatternExecuteInterface interface, FlatPattern flat) {
    if(interface.stack.depth() != flat.stack_arg_count) std::terminate(); //not good
    auto& arena = interface.arena;
    auto stack = interface.stack;
    auto outer_head = arena.copy(flat.head);
    std::vector<new_expression::PatternStep> steps;
    for(std::size_t i = 0; i < flat.stack_arg_count; ++i) {
      outer_head = arena.apply(
        std::move(outer_head),
        arena.argument(i)
      );
      steps.push_back(new_expression::PullArgument{});
    }
    std::size_t next_arg = flat.stack_arg_count;
    auto pull_argument = [&] {
      auto arg_index = next_arg++;
      auto next_type = extract_application_domain(arena, stack, interface.arrow, outer_head, arena.argument(arg_index));
      stack = stack.extend(std::move(next_type));
      steps.push_back(new_expression::PullArgument{});
    };
    for(auto& shard : flat.shards) {
      if(auto* match = std::get_if<FlatPatternMatch>(&shard)) {
        new_expression::OwnedExpression head_expr = std::visit(mdb::overloaded{
          [&](FlatPatternMatchArg&& arg) {
            return arena.argument(arg.matched_arg_index);
          },
          [&](FlatPatternMatchSubexpression&& subexpr) {
            return interface.get_subexpression(subexpr.matched_subexpression, stack, std::move(subexpr.requested_captures));
          }
        }, std::move(match->matched_expr));
        auto shard_head = std::move(match->match_head);
        for(std::size_t i = 0; i < match->capture_count; ++i) {
          auto next_type = extract_application_domain(arena, stack, interface.arrow, shard_head, arena.argument(next_arg++));
          stack = stack.extend(std::move(next_type));
        }
        steps.push_back(new_expression::PatternMatch{
          .substitution = arena.copy(head_expr),
          .expected_head = arena.copy(match->match_head),
          .args_captured = match->capture_count
        });
        stack = extend_by_assumption_typeless(std::move(stack), std::move(head_expr), std::move(shard_head));
      } else {
        if(!std::holds_alternative<FlatPatternPullArgument>(shard)) std::terminate();
        pull_argument();
      }
    }
    std::vector<std::pair<new_expression::OwnedExpression, new_expression::OwnedExpression> > checks;
    for(auto& check : flat.checks) {
      checks.emplace_back(
        arena.argument(check.arg_index),
        std::move(as_expression(arena, std::move(check.expected_value), flat.captures))
      );
    }
    auto type_of_pattern = stack.type_of(outer_head);
    arena.drop(std::move(outer_head));
    return {
      .pattern = {
        .head = std::move(flat.head),
        .body = {
          .args_captured = flat.arg_count,
          .steps = std::move(steps)
        }
      },
      .type_of_pattern = std::move(type_of_pattern),
      .captures = std::move(flat.captures),
      .checks = std::move(checks),
      .pattern_stack = std::move(stack)
    };
  }
}
