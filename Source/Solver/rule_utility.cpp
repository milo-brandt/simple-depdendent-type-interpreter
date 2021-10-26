#include "rule_utility.hpp"
#include "../NewExpression/arena_utility.hpp"
#include "../Utility/vector_utility.hpp"

namespace solver {
  namespace pattern_expr {
    void destroy_from_arena(new_expression::Arena& arena, PatternExpr& expr) {
      expr.visit(mdb::overloaded{
        [&](Apply& apply) {
          destroy_from_arena(arena, apply.lhs);
          destroy_from_arena(arena, apply.rhs);
        },
        [&](Embed& embed) {
          arena.drop(std::move(embed.value));
        },
        [&](Capture&) {},
        [&](Hole&) {}
      });
    }
  }
  namespace pattern_expr::archive_root {
    void destroy_from_arena(new_expression::Arena& arena, PatternExpr& expr_root) {
      struct Detail {
        new_expression::Arena& arena;
        void destroy(pattern_expr::archive_part::PatternExpr& expr) {
          expr.visit(mdb::overloaded{
            [&](pattern_expr::archive_part::Apply& apply) {
              destroy(apply.lhs);
              destroy(apply.rhs);
            },
            [&](pattern_expr::archive_part::Embed& embed) {
              arena.drop(std::move(embed.value));
            },
            [&](pattern_expr::archive_part::Capture&) {},
            [&](pattern_expr::archive_part::Hole&) {}
          });
        }
      };
      Detail{arena}.destroy(expr_root.root());
    }
  }
  namespace pattern_node::archive_root {
    void destroy_from_arena(new_expression::Arena& arena, PatternNode& node_root) {
      struct Detail {
        new_expression::Arena& arena;
        void destroy(pattern_node::archive_part::PatternNode& node) {
          node.visit(mdb::overloaded{
            [&](pattern_node::archive_part::Apply& apply) {
              arena.drop(std::move(apply.head));
              for(auto& arg : apply.args) {
                destroy(arg);
              }
            },
            [&](pattern_node::archive_part::Capture&) {},
            [&](pattern_node::archive_part::Check& check) {
              destroy_from_arena(arena, check.desired_equality);
            },
            [&](pattern_node::archive_part::Ignore&) {}
          });
        }
      };
      Detail{arena}.destroy(node_root.root());
    }
  }
  namespace instruction_archive = compiler::new_instruction::output::archive_part;
  pattern_located_resolution::PatternExpr resolve_pattern_impl(compiler::new_instruction::output::archive_part::Pattern const& pattern, PatternResolveInterface const& interface) {
    return pattern.visit(mdb::overloaded{
      [&](instruction_archive::PatternApply const& apply) -> pattern_located_resolution::PatternExpr {
        return pattern_located_resolution::Apply{
          .lhs = resolve_pattern_impl(apply.lhs, interface),
          .rhs = resolve_pattern_impl(apply.rhs, interface),
          .source = apply.index()
        };
      },
      [&](instruction_archive::PatternLocal const& local) -> pattern_located_resolution::PatternExpr {
        return pattern_located_resolution::Embed{
          .value = interface.lookup_local(local.local_index),
          .source = local.index()
        };
      },
      [&](instruction_archive::PatternEmbed const& embed) -> pattern_located_resolution::PatternExpr {
        return pattern_located_resolution::Embed{
          .value = interface.lookup_embed(embed.embed_index),
          .source = embed.index()
        };
      },
      [&](instruction_archive::PatternCapture const& capture) -> pattern_located_resolution::PatternExpr {
        return pattern_located_resolution::Capture {
          .capture_index = capture.capture_index,
          .source = capture.index()
        };
      },
      [&](instruction_archive::PatternHole const& hole) -> pattern_located_resolution::PatternExpr {
        return pattern_located_resolution::Hole{
          .source = hole.index()
        };
      }
    });
  }
  ResolutionResult resolve_pattern(compiler::new_instruction::output::archive_part::Pattern const& pattern, PatternResolveInterface const& interface) {
    auto raw = resolve_pattern_impl(pattern, interface);
    return {
      .output = archive(std::move(raw.output)),
      .locator = archive(std::move(raw.locator))
    };
  }
  namespace {
    struct PatternExprUnfolding {
      pattern_expr::archive_part::PatternExpr* head;
      std::vector<pattern_expr::archive_part::PatternExpr*> args;
    };
    PatternExprUnfolding unfold(pattern_expr::archive_part::PatternExpr& expr) {
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
    pattern_expr::PatternExpr move_from_archive(pattern_expr::archive_part::PatternExpr& expr) {
      return expr.visit(mdb::overloaded{
        [&](pattern_expr::archive_part::Apply& apply) -> pattern_expr::PatternExpr {
          return pattern_expr::Apply{
            move_from_archive(apply.lhs),
            move_from_archive(apply.rhs)
          };
        },
        [&](pattern_expr::archive_part::Embed& embed) -> pattern_expr::PatternExpr {
          return pattern_expr::Embed{
            .value = std::move(embed.value)
          };
        },
        [&](pattern_expr::archive_part::Capture& capture) -> pattern_expr::PatternExpr {
          return pattern_expr::Capture{
            .capture_index = capture.capture_index
          };
        },
        [&](pattern_expr::archive_part::Hole& hole) -> pattern_expr::PatternExpr {
          return pattern_expr::Hole{};
        }
      });
    }
    pattern_located_normalization::PatternNode convert_to_node(new_expression::Arena& arena, pattern_expr::archive_part::PatternExpr& expr) {
      auto pattern_unfolded = unfold(expr);
      auto get_check = [&]() -> pattern_located_normalization::PatternNode {
        return pattern_located_normalization::Check{
          .desired_equality = move_from_archive(expr),
          .source = expr.index()
        };
      };
      return pattern_unfolded.head->visit(mdb::overloaded{
        [&](pattern_expr::archive_part::Apply& apply) -> pattern_located_normalization::PatternNode {
          std::terminate(); //unreachable
        },
        [&](pattern_expr::archive_part::Embed& embed) -> pattern_located_normalization::PatternNode {
          auto unfolded = unfold(arena, embed.value);
          if(arena.holds_axiom(unfolded.head)) {
            std::vector<pattern_located_normalization::PatternNode> pattern_nodes;
            for(auto& arg : unfolded.args) {
              pattern_nodes.push_back(pattern_located_normalization::Check{
                .desired_equality = pattern_expr::Embed{
                  .value = arena.copy(arg),
                },
                .source = pattern_unfolded.head->index()
              });
            }
            for(auto* pat_arg : pattern_unfolded.args) {
              pattern_nodes.push_back(convert_to_node(arena, *pat_arg));
            }
            auto head = arena.copy(unfolded.head);
            arena.drop(std::move(embed.value));
            return pattern_located_normalization::Apply{
              .args = std::move(pattern_nodes),
              .head = std::move(head),
              .source = expr.index()
            };
          } else {
            return get_check();
          }
        },
        [&](pattern_expr::archive_part::Capture& capture) -> pattern_located_normalization::PatternNode {
          if(pattern_unfolded.args.size() > 0) {
            return get_check();
          } else {
            return pattern_located_normalization::Capture{
              .capture_index = capture.capture_index,
              .source = capture.index()
            };
          }
        },
        [&](pattern_expr::archive_part::Hole& hole) -> pattern_located_normalization::PatternNode {
          if(pattern_unfolded.args.size() > 0) {
            return get_check();
          } else {
            return pattern_located_normalization::Ignore{
              .source = hole.index()
            };
          }
        }
      });
    }
  }
  mdb::Result<NormalizationResult, NormalizationError> normalize_pattern(new_expression::Arena& arena, RawPattern raw, std::size_t capture_count) {
    //step 1: figure out how many arguments are on head.
    auto head_pattern_unfolding = unfold(raw.primary_pattern.root());
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
    FoldedPatternBacktrace backtrace;
    std::vector<pattern_node::archive_root::PatternNode> matches;
    for(auto* arg : head_pattern_unfolding.args) {
      auto node = convert_to_node(arena, *arg);
      matches.push_back(archive(std::move(node.output)));
      backtrace.matches.push_back(archive(std::move(node.locator)));
    }
    std::vector<FoldedSubclause> subclause_matches;
    NormalizationError errors;
    std::size_t subpattern_index = 0;
    for(auto& shard : raw.subpatterns) {
      auto node = convert_to_node(arena, shard.pattern.root());
      if(!node.output.holds_apply()) {
        errors.nonmatchable_subclauses.push_back(subpattern_index);
      }
      subclause_matches.push_back({
        .used_captures = std::move(shard.used_captures),
        .node = archive(std::move(node.output))
      });
      backtrace.subclause_matches.push_back(archive(std::move(node.locator)));
      ++subpattern_index;
    }
    if(errors.nonmatchable_subclauses.empty()) {
      return NormalizationResult{
        .output = FoldedPattern{
          .head = std::move(head),
          .stack_arg_count = arg_count,
          .capture_count = capture_count,
          .matches = std::move(matches),
          .subclause_matches = std::move(subclause_matches)
        },
        .locator = std::move(backtrace)
      };
    } else {
      destroy_from_arena(arena, head, matches, subclause_matches);
      return std::move(errors);
    }
  }
  mdb::Result<FlatResult, FlatError> flatten_pattern(new_expression::Arena& arena, FoldedPattern folded) {
    struct Detail {
      struct CaptureInfo {
        std::size_t arg_index;
        PatternNodeIndex<pattern_node_archive_index::Capture> primary_capture;
      };
      new_expression::Arena& arena;
      std::vector<std::optional<CaptureInfo> > capture_positions;
      std::size_t next_arg;
      std::vector<FlatPatternShard> shards;
      std::vector<FlatPatternShardExplanation> shard_explanations;
      std::vector<FlatPatternCheck> checks;
      std::vector<FlatPatternCheckExplanation> check_explanations;
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
          return arena.argument(position->arg_index);
        }, requested);
      }
      void process(FlatPatternPart subclause_index, pattern_node::archive_part::PatternNode& node, FlatPatternMatchExpr subexpr) {
        node.visit(mdb::overloaded{
          [&](pattern_node::archive_part::Apply& apply) {
            shards.push_back(FlatPatternMatch{
              .matched_expr = std::move(subexpr),
              .match_head = std::move(apply.head),
              .capture_count = apply.args.size()
            });
            shard_explanations.push_back(FlatPatternMatchExplanation{
              .source = {
                subclause_index,
                apply.index()
              }
            });
            auto arg_pos = next_arg;
            next_arg += apply.args.size();
            for(auto& arg : apply.args) {
              process(subclause_index, arg, FlatPatternMatchArg{arg_pos++});
            }
          },
          [&](pattern_node::archive_part::Capture& capture) {
            //this function will only be called for MatchArgs, never Subexpressions
            auto index = std::get<0>(subexpr).matched_arg_index;
            if(capture_positions[capture.capture_index]) {
              checks.push_back({
                .arg_index = index,
                .expected_value = pattern_expr::Capture{
                  capture.capture_index
                }
              });
              check_explanations.push_back(FlatPatternDoubleCaptureCheck{
                .primary_capture = capture_positions[capture.capture_index]->primary_capture,
                .secondary_capture = {
                  subclause_index,
                  capture.index()
                }
              });
            } else {
              capture_positions[capture.capture_index] = CaptureInfo{
                .arg_index = index,
                .primary_capture = {
                  subclause_index,
                  capture.index()
                }
              };
            }
          },
          [&](pattern_node::archive_part::Check& check) {
            auto index = std::get<0>(subexpr).matched_arg_index;
            checks.push_back({
              .arg_index = index,
              .expected_value = std::move(check.desired_equality)
            });
            check_explanations.push_back(FlatPatternInlineCheck{
              .source = {
                subclause_index,
                check.index()
              }
            });
          },
          [&](pattern_node::archive_part::Ignore&) {
            //nothing to do
            if(subexpr.index() != 0) std::terminate(); //can't pass a non-trivial expression in here
          }
        });
      }
      bool has_all_captures() {
        for(auto const& capture : capture_positions) {
          if(!capture) return false;
        }
        return true;
      }
      std::vector<new_expression::OwnedExpression> get_captures() {
        return mdb::map([&](std::optional<CaptureInfo> const& position) {
          if(!position) std::terminate();
          return arena.argument(position->arg_index);
        }, capture_positions);
      }
    };
    auto arg_count = folded.stack_arg_count + folded.matches.size();
    Detail detail{
      .arena = arena,
      .capture_positions = std::vector<std::optional<Detail::CaptureInfo> >(folded.capture_count, std::optional<Detail::CaptureInfo>{}),
      .next_arg = folded.stack_arg_count
    };
    std::size_t args_pulled = 0;
    auto pull_argument = [&] {
      detail.shards.push_back(FlatPatternPullArgument{});
      detail.shard_explanations.push_back(FlatPatternPullArgumentExplanation{});
      auto index = args_pulled++;
      detail.process(
        FlatPatternPart{
          .primary = true,
          .index = index
        },
        folded.matches[index].root(),
        FlatPatternMatchArg{detail.next_arg++}
      );
    };
    auto can_pull_argument = [&] {
      return args_pulled != folded.matches.size();
    };

    for(std::size_t sub_index = 0; sub_index < folded.subclause_matches.size(); ++sub_index) {
      auto& sub = folded.subclause_matches[sub_index];
    TRY_TO_INSTANTIATE_SUBCLAUSE:
      if(detail.has_needed_captures(sub.used_captures)) {
        auto head = FlatPatternMatchSubexpression{
          .requested_captures = detail.get_captures_for(sub.used_captures),
          .matched_subexpression = sub_index
        };
        detail.process(
          FlatPatternPart{
            .primary = false,
            .index = sub_index
          },
          sub.node.root(),
          std::move(head)
        );
      } else {
        if(can_pull_argument()) {
          pull_argument();
          goto TRY_TO_INSTANTIATE_SUBCLAUSE;
        } else {
          //failed - unreachable capture
          for(std::size_t destroy_index = sub_index; destroy_index < folded.subclause_matches.size(); ++destroy_index) {
            destroy_from_arena(arena, folded.subclause_matches[destroy_index]);
          }
          destroy_from_arena(arena, folded.head, detail.shards, detail.checks);
          return {mdb::in_place_error, FlatSubclauseMissingCapture{
            .subclause_index = sub_index
          }};
        }
      }
    }
    while(can_pull_argument()) pull_argument();
    if(!detail.has_all_captures()) {
      destroy_from_arena(arena, folded.head, detail.shards, detail.checks);
      return {mdb::in_place_error, FlatGlobalMissingCapture{}};
    }
    return FlatResult{
      .output = FlatPattern {
        .head = std::move(folded.head),
        .stack_arg_count = folded.stack_arg_count,
        .arg_count = arg_count,
        .shards = std::move(detail.shards),
        .captures = detail.get_captures(),
        .checks = std::move(detail.checks)
      },
      .locator = FlatPatternExplanation {
        .shards = std::move(detail.shard_explanations),
        .checks = std::move(detail.check_explanations)
      }
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
          //TODO SOON: Deal with all of this - and actually type check it!
          std::terminate(); //can't yet deal with holes appearing!
        }
      });
    }
    std::optional<new_expression::OwnedExpression> extract_application_domain(new_expression::Arena& arena, Stack& stack, new_expression::WeakExpression arrow, new_expression::OwnedExpression& fn, new_expression::OwnedExpression arg) {
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
        return std::nullopt;
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
  mdb::Result<PatternExecutionResult, PatternExecuteError> execute_pattern(PatternExecuteInterface interface, FlatPattern flat) {
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
      if(auto next_type = extract_application_domain(arena, stack, interface.arrow, outer_head, arena.argument(arg_index))) {
        stack = stack.extend(std::move(*next_type));
        steps.push_back(new_expression::PullArgument{});
        return true;
      } else {
        return false;
      }
    };
    std::size_t shard_index = 0;
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
          if(auto next_type = extract_application_domain(arena, stack, interface.arrow, shard_head, arena.argument(next_arg++))) {
            stack = stack.extend(std::move(*next_type));
          } else {
            return {mdb::in_place_error, PatternExecuteShardNotFunction{
              .shard_index = shard_index,
              .args_applied = i
            }};
          }
        }
        steps.push_back(new_expression::PatternMatch{
          .substitution = arena.copy(head_expr),
          .expected_head = arena.copy(match->match_head),
          .args_captured = match->capture_count
        });
        stack = extend_by_assumption_typeless(std::move(stack), std::move(head_expr), std::move(shard_head));
      } else {
        if(!std::holds_alternative<FlatPatternPullArgument>(shard)) std::terminate();
        if(!pull_argument()) {
          return {mdb::in_place_error, PatternExecuteGlobalNotFunction{}};
        }
      }
      shard_index++;
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
    return PatternExecutionResult{
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
