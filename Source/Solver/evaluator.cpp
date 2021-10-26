#include "evaluator.hpp"
#include "../NewExpression/arena_utility.hpp"
#include "rule_utility.hpp"
#include "common_routines.hpp"

namespace solver::evaluator {
  using TypedValue = new_expression::TypedValue;
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Stack = stack::Stack;
  namespace instruction_archive = compiler::new_instruction::output::archive_part;
  enum class VariableKind {
    axiom,
    declaration,
    indeterminate
  };
  struct ExpressionEvaluateDetail {
    EvaluatorInterface interface;
    std::vector<TypedValue> locals;

    template<VariableKind kind>
    TypedValue make_variable_typed(OwnedExpression type, Stack& local_context, variable_explanation::Any explanation) {
      //auto true_type = local_context.instance_of_type_family(interface.arena.copy(type));
      auto var = [&] {
        if constexpr(kind == VariableKind::axiom) {
          return interface.arena.axiom();
        } else {
          return interface.arena.declaration();
        }
      }();
      interface.register_type(var, local_context.instance_of_type_family(interface.arena.copy(type)));
      interface.register_declaration(var);
      interface.explain_variable(var, std::move(explanation));
      if constexpr(kind == VariableKind::indeterminate) {
        interface.register_definable_indeterminate(interface.arena.copy(var), local_context);
      }
      return {.value = local_context.apply_args(std::move(var)), .type = std::move(type)};
    }
    template<VariableKind kind>
    OwnedExpression make_variable(OwnedExpression type, Stack& local_context, variable_explanation::Any explanation) {
      auto typed = make_variable_typed<kind>(std::move(type), local_context, std::move(explanation));
      interface.arena.drop(std::move(typed.type));
      return std::move(typed.value);
    }
    template<class Then>
    auto call_on_error(EquationResult bad_result, Then&& then) {
      if(auto* stall = std::get_if<EquationStalled>(&bad_result)) {
        then(std::move(stall->error));
      } else {
        std::move(std::get<EquationFailed>(bad_result).error).listen(std::forward<Then>(then));
      }
    }
    template<class ErrorType, class T>
    auto error_listener(T first_arg) {
      return [this, first_arg](EquationResult result) {
        if(auto* stall = std::get_if<EquationStalled>(&result)) {
          interface.report_error(ErrorType{
            first_arg,
            std::move(stall->error)
          });
        } else if(auto* fail = std::get_if<EquationFailed>(&result)) {
          std::move(fail->error).listen([this, first_arg = std::move(first_arg)](EquationErrorInfo error) {
            interface.report_error(ErrorType{
              first_arg,
              std::move(error)
            });
          });
        }
      };
    }
    template<class Listener>
    TypedValue cast_typed(TypedValue input, OwnedExpression new_type, Stack& local_context, variable_explanation::Any cast_var_explanation, Listener&& listener) {
      input.type = local_context.reduce(std::move(input.type));
      new_type = local_context.reduce(std::move(new_type));
      if(input.type == new_type) {
        interface.arena.drop(std::move(new_type));
        return std::move(input);
      }
      auto cast_var = make_variable<VariableKind::declaration>(interface.arena.copy(new_type), local_context, std::move(cast_var_explanation));
      auto var = new_expression::unfold(interface.arena, cast_var).head;
      routine::cast(routine::CastInfo{
        .stack = local_context,
        .variable = interface.arena.copy(var),
        .source_type = std::move(input.type),
        .source = std::move(input.value),
        .target_type = interface.arena.copy(new_type),
        .arena = interface.arena,
        .handle_equation = mdb::ref(interface.solve),
        .add_rule = mdb::ref(interface.add_rule)
      }).listen(std::forward<Listener>(listener));
      return {
        .value = std::move(cast_var),
        .type = std::move(new_type)
      };
    }
    template<class Listener>
    OwnedExpression cast(TypedValue input, OwnedExpression new_type, Stack& local_context, variable_explanation::Any cast_var_explanation, Listener&& listener) {
      auto typed = cast_typed(std::move(input), std::move(new_type), local_context, std::move(cast_var_explanation), std::forward<Listener>(listener));
      interface.arena.drop(std::move(typed.type));
      return std::move(typed.value);
    }
    using ExpressionResult = TypedValue;
    ExpressionResult evaluate(instruction_archive::Expression const& tree, Stack& local_context) {
      return tree.visit(mdb::overloaded{
        [&](instruction_archive::Apply const& apply) -> ExpressionResult {
          auto lhs = evaluate(apply.lhs, local_context);
          auto rhs = evaluate(apply.rhs, local_context);
          auto [lhs_value, lhs_codomain, rhs_value] = [&]() -> std::tuple<OwnedExpression, OwnedExpression, OwnedExpression> {
            //this function should consume lhs and rhs.
            lhs.type = local_context.reduce(std::move(lhs.type));
            auto lhs_unfolded = unfold(interface.arena, lhs.type);
            if(lhs_unfolded.head == interface.arrow && lhs_unfolded.args.size() == 2) {
              new_expression::RAIIDestroyer destroyer{interface.arena, lhs.type};
              return std::make_tuple(
                std::move(lhs.value),
                interface.arena.copy(lhs_unfolded.args[1]),
                cast(
                  std::move(rhs),
                  interface.arena.copy(lhs_unfolded.args[0]),
                  local_context,
                  variable_explanation::ApplyRHSCast{apply.index()},
                  error_listener<error::MismatchedArgType>(apply.index())
                )
              );
            }
            auto domain = make_variable<VariableKind::indeterminate>(
              interface.arena.copy(interface.type),
              local_context,
              variable_explanation::ApplyDomain{apply.index()}
            );
            auto codomain = make_variable<VariableKind::indeterminate>(
              interface.arena.apply(
                interface.arena.copy(interface.type_family),
                interface.arena.copy(domain)
              ),
              local_context,
              variable_explanation::ApplyCodomain{apply.index()}
            );
            auto expected_function_type = interface.arena.apply(
              interface.arena.copy(interface.arrow),
              interface.arena.copy(domain),
              interface.arena.copy(codomain)
            );
            auto func = make_variable<VariableKind::declaration>(
              interface.arena.copy(expected_function_type),
              local_context,
              variable_explanation::ApplyLHSCast{apply.index()}
            );
            auto arg = make_variable<VariableKind::declaration>(
              interface.arena.copy(domain),
              local_context,
              variable_explanation::ApplyRHSCast{apply.index()}
            );
            auto func_var = unfold(interface.arena, func).head;
            auto arg_var = unfold(interface.arena, arg).head;
            routine::function_cast(routine::FunctionCastInfo{
              .stack = local_context,
              .function_variable = interface.arena.copy(func_var),
              .argument_variable = interface.arena.copy(arg_var),
              .function_value = std::move(lhs.value),
              .function_type = std::move(lhs.type),
              .expected_function_type = std::move(expected_function_type),
              .argument_value = std::move(rhs.value),
              .argument_type = std::move(rhs.type),
              .expected_argument_type = std::move(domain),
              .arena = interface.arena,
              .handle_equation = mdb::ref(interface.solve),
              .add_rule = mdb::ref(interface.add_rule)
            }).listen([this, index = apply.index()](routine::FunctionCastResult result) {
              if(result.lhs_was_function) {
                error_listener<error::MismatchedArgType>(index)(std::move(result.result));
              } else {
                error_listener<error::NotAFunction>(index)(std::move(result.result));
              }
            });
            return std::make_tuple(
              std::move(func),
              std::move(codomain),
              std::move(arg)
            );
          } ();
          return TypedValue{
            .value = interface.arena.apply(std::move(lhs_value), interface.arena.copy(rhs_value)),
            .type = interface.arena.apply(std::move(lhs_codomain), std::move(rhs_value))
          };
        },
        [&](instruction_archive::Local const& local) -> ExpressionResult {
          return copy_on_arena(interface.arena, locals.at(local.local_index));
        },
        [&](instruction_archive::Embed const& e) -> ExpressionResult {
          return interface.embed(e.embed_index);
        },
        [&](instruction_archive::PrimitiveExpression const& primitive) -> ExpressionResult {
          switch(primitive.primitive) {
            case compiler::new_instruction::Primitive::type: return TypedValue{
              .value = interface.arena.copy(interface.type),
              .type = interface.arena.copy(interface.type)
            };
            case compiler::new_instruction::Primitive::arrow: return {
              .value = interface.arena.copy(interface.arrow),
              .type = interface.arena.copy(interface.arrow_type)
            };
            //case instruction::Primitive::push_vec: return {expression_context.get_external(expression_context.primitives.push_vec), forward_locator::PrimitiveExpression{}};
            //case instruction::Primitive::empty_vec: return {expression_context.get_external(expression_context.primitives.empty_vec), forward_locator::PrimitiveExpression{}};
            default: std::terminate();
          }
          std::terminate();
        },
        [&](instruction_archive::TypeFamilyOver const& type_family) -> ExpressionResult {
          auto type_family_type = evaluate(type_family.type, local_context);
          auto type = cast(
            std::move(type_family_type),
            interface.arena.copy(interface.type),
            local_context,
            variable_explanation::TypeFamilyCast{type_family.index()},
            error_listener<error::BadTypeFamilyType>(type_family.index())
          );
          return {
            .value = interface.arena.apply(
              interface.arena.copy(interface.type_family),
              std::move(type)
            ),
            .type = interface.arena.copy(interface.type)
          };
        }
      });
    }
    template<VariableKind kind, class Listener>
    void create_declaration(instruction_archive::Expression const& type, Stack& local_context, variable_explanation::Any cast_explanation, variable_explanation::Any var_explanation, Listener&& listener) {
      auto type_eval = evaluate(type, local_context);
      auto value = make_variable_typed<kind>(
        cast(
          std::move(type_eval),
          interface.arena.copy(interface.type),
          local_context,
          std::move(cast_explanation),
          std::forward<Listener>(listener)
        ),
        local_context,
        std::move(var_explanation)
      );
      locals.push_back(std::move(value));
    }
    void evaluate(instruction_archive::Command const& command, Stack& local_context) {
      return command.visit(mdb::overloaded{
        [&](instruction_archive::DeclareHole const& hole) {
          return create_declaration<VariableKind::indeterminate>(
            hole.type,
            local_context,
            variable_explanation::HoleTypeCast{hole.index()},
            variable_explanation::ExplicitHole{hole.index()},
            error_listener<error::BadHoleType>(hole.index())
          );
        },
        [&](instruction_archive::Declare const& declare) {
          return create_declaration<VariableKind::declaration>(
            declare.type,
            local_context,
            variable_explanation::DeclareTypeCast{declare.index()},
            variable_explanation::Declaration{declare.index()},
            error_listener<error::BadDeclarationType>(declare.index())
          );
        },
        [&](instruction_archive::Axiom const& axiom) {
          return create_declaration<VariableKind::axiom>(
            axiom.type,
            local_context,
            variable_explanation::AxiomTypeCast{axiom.index()},
            variable_explanation::Axiom{axiom.index()},
            error_listener<error::BadAxiomType>(axiom.index())
          );
        },
        [&](instruction_archive::Rule const& rule) {
          PatternResolveInterface resolve_interface{
            .lookup_local = [&](std::uint64_t index) {
              return interface.arena.copy(locals.at(index).value);
            },
            .lookup_embed = [&](std::uint64_t index) {
              auto typed = interface.embed(index);
              interface.arena.drop(std::move(typed.type));
              return std::move(typed.value);
            }
          };
          auto resolved_result = resolve_pattern(rule.primary_pattern, resolve_interface);
          auto& resolved = resolved_result.output;
          std::vector<RawPatternShard> subpatterns;
          std::vector<pattern_resolution_locator::archive_root::PatternExpr> subpattern_locators;
          for(auto const& submatch_generic : rule.submatches) {
            auto const& submatch = submatch_generic.get_submatch();
            auto resolution = resolve_pattern(submatch.pattern, resolve_interface);
            subpatterns.push_back({
              .used_captures = submatch.captures_used,
              .pattern = std::move(resolution.output)
            });
            subpattern_locators.push_back(std::move(resolution.locator));
          }
          auto folded_result = normalize_pattern(interface.arena, RawPattern{
            .primary_pattern = std::move(resolved),
            .subpatterns = std::move(subpatterns)
          }, rule.capture_count);
          if(folded_result.holds_error()) {
            for(std::size_t nonmatchable_subclause : folded_result.get_error().nonmatchable_subclauses) {
              interface.report_error(error::NonmatchableSubclause{
                .rule = rule.index(),
                .subclause = rule.submatches[nonmatchable_subclause].get_submatch().matched_expression.index()
              });
            }
            return;
          }
          auto& folded = folded_result.get_value().output;
          auto flat_result = flatten_pattern(interface.arena, std::move(folded));
          if(flat_result.holds_error()) {
            auto& err = flat_result.get_error();
            if(auto* subclause = std::get_if<FlatSubclauseMissingCapture>(&err)) {
              interface.report_error(error::MissingCaptureInSubclause{
                .rule = rule.index(),
                .subclause = rule.submatches[subclause->subclause_index].get_submatch().matched_expression.index()
              });
            } else {
              interface.report_error(error::MissingCaptureInRule{
                .rule = rule.index()
              });
            }
            return;
          }
          auto& flat = flat_result.get_value().output;
          auto executed_result = execute_pattern({
            .arena = interface.arena,
            .arrow = interface.arrow,
            .stack = local_context,
            .get_subexpression = [&](std::size_t index, Stack expr_context, std::vector<OwnedExpression> requested_captures) {
              auto locals_size_before = locals.size();
              for(auto& capture : requested_captures) {
                locals.push_back({
                  std::move(capture),
                  expr_context.type_of(capture)
                });
              }
              auto const& inst = rule.submatches[index].get_submatch();
              for(auto const& command : inst.matched_expression_commands) {
                evaluate(command, expr_context);
              }
              auto value = evaluate(inst.matched_expression, expr_context);
              for(std::size_t i = locals_size_before; i < locals.size(); ++i) {
                destroy_from_arena(interface.arena, locals[i]);
              }
              locals.erase(locals.begin() + locals_size_before, locals.end());
              interface.arena.drop(std::move(value.type));
              return std::move(value.value);
            }
          }, std::move(flat));
          if(executed_result.holds_error()) {
            auto& err = executed_result.get_error();
            if(auto* shard = std::get_if<PatternExecuteShardNotFunction>(&err)) {
              auto const& shard_explanation = flat_result.get_value().locator.shards[shard->shard_index];
              auto const& part = std::get<FlatPatternMatchExplanation>(shard_explanation).source.part;
              auto subclause_index = std::get<FlatPatternMatchExplanation>(shard_explanation).source.part.index;
              if(part.primary) {
                interface.report_error(error::BadApplicationInPattern{
                  .rule = rule.index()
                });
              } else {
                interface.report_error(error::BadApplicationInSubclause{
                  .rule = rule.index(),
                  .subclause = rule.submatches[part.index].get_submatch().pattern.index()
                });
              }
            } else {
              interface.report_error(error::BadApplicationInPattern{
                .rule = rule.index()
              });
            }
            return;
          }
          auto& executed = executed_result.get_value();
          auto locals_size_before = locals.size();
          for(auto& capture : executed.captures) {
            locals.push_back({
              std::move(capture),
              executed.pattern_stack.type_of(capture)
            });
          }
          for(auto const& command : rule.commands) {
            evaluate(command, executed.pattern_stack);
          }
          auto replacement = evaluate(rule.replacement, executed.pattern_stack);
          for(std::size_t i = locals_size_before; i < locals.size(); ++i) {
            destroy_from_arena(interface.arena, locals[i]);
          }
          locals.erase(locals.begin() + locals_size_before, locals.end());
          new_expression::Rule proposed_rule{
            .pattern = std::move(executed.pattern),
            .replacement = std::move(executed.pattern_stack.eliminate_conglomerates(
              std::move(replacement.value)
            ))
          };
          struct BacktraceInfo {
            new_expression::Arena& arena;
            instruction_archive::Rule const& rule;
            pattern_resolution_locator::archive_root::PatternExpr primary_pattern_resolution_locator;
            std::vector<pattern_resolution_locator::archive_root::PatternExpr> subclause_resolution_locators;
            FoldedPatternBacktrace folded_locator;
            FlatPatternExplanation flat_locator;
            std::size_t equations_left;
            new_expression::Rule proposed_rule;
            error::Any get_error(std::size_t check_index, EquationErrorInfo error_info) {
              auto lookup = [&]<class T>(PatternNodeIndex<T> index) -> decltype(auto) {
                if(index.part.primary) {
                  return primary_pattern_resolution_locator[folded_locator.matches.at(index.part.index)[index.archive_index].source];
                } else {
                  return subclause_resolution_locators[index.part.index][folded_locator.subclause_matches.at(index.part.index)[index.archive_index].source];
                }
              };
              auto lookup_expr = [&]<class T>(PatternNodeIndex<T> index) -> decltype(auto) {
                if(index.part.primary) {
                  return primary_pattern_resolution_locator[index.archive_index].source;
                } else {
                  return subclause_resolution_locators[index.part.index][index.archive_index].source;
                }
              };
              auto const& reason = flat_locator.checks[check_index];
              return std::visit(mdb::overloaded{
                [&](FlatPatternDoubleCaptureCheck const& double_capture) -> error::Any {
                  return error::InvalidDoubleCapture {
                    .rule = rule.index(),
                    .primary_capture = lookup(double_capture.primary_capture).source,
                    .secondary_capture = lookup(double_capture.secondary_capture).source,
                    .equation = std::move(error_info)
                  };
                },
                [&](FlatPatternInlineCheck const& inline_check) -> error::Any {
                  return error::InvalidNondestructurablePattern {
                    .rule = rule.index(),
                    .pattern_part = lookup(inline_check.source).visit([&](auto const& x) -> compiler::new_instruction::archive_index::Pattern { return x.source; }),
                    .equation = std::move(error_info)
                  };
                }
              }, reason);
            }
            ~BacktraceInfo() {
              if(equations_left > 0) {
                destroy_from_arena(arena, proposed_rule);
              }
            }
          };
          std::shared_ptr<BacktraceInfo> backtrace_info{new BacktraceInfo{
            .arena = interface.arena,
            .rule = rule,
            .primary_pattern_resolution_locator = std::move(resolved_result.locator),
            .subclause_resolution_locators = std::move(subpattern_locators),
            .folded_locator = std::move(folded_result.get_value().locator),
            .flat_locator = std::move(flat_result.get_value().locator),
            .equations_left = 1 + executed.checks.size(),
            .proposed_rule = std::move(proposed_rule)
          }};
          std::size_t check_index = 0;
          for(auto& check : executed.checks) {
            interface.solve({
              .lhs = std::move(check.first),
              .rhs = std::move(check.second),
              .stack = executed.pattern_stack
            }).listen([this, index = check_index++, backtrace_info](EquationResult result) {
              if(std::holds_alternative<EquationSolved>(result)) {
                if(--backtrace_info->equations_left == 0) {
                  interface.add_rule(std::move(backtrace_info->proposed_rule));
                }
              } else {
                call_on_error(std::move(result), [this, index, backtrace_info](EquationErrorInfo error_info) {
                  interface.report_error(backtrace_info->get_error(index, std::move(error_info)));
                });
              }
            });
          }
          interface.solve({
            .lhs = std::move(executed.type_of_pattern),
            .rhs = std::move(replacement.type),
            .stack = executed.pattern_stack
          }).listen([this, backtrace_info](EquationResult result) {
            if(std::holds_alternative<EquationSolved>(result)) {
              if(--backtrace_info->equations_left == 0) {
                interface.add_rule(std::move(backtrace_info->proposed_rule));
              }
            } else {
              call_on_error(std::move(result), [this, backtrace_info](EquationErrorInfo error_info) {
                interface.report_error(error::MismatchedReplacementType{
                  .rule = backtrace_info->rule.index(),
                  .equation = std::move(error_info)
                });
              });
            }
          });
        },
        [&](instruction_archive::Check const& check) {
          auto& solver = check.allow_deduction ? interface.solve : interface.solve_no_deduce;
          auto term = evaluate(check.term, local_context);
          std::optional<OwnedExpression> expected_type;
          if(check.expected_type) {
            expected_type = cast(
              evaluate(*check.expected_type, local_context),
              interface.arena.copy(interface.type),
              local_context,
              variable_explanation::RequirementTypeCast{check.index()},
              error_listener<error::BadRequirementType>(check.index())
            );
          }
          std::optional<TypedValue> expected;
          if(check.expected) {
            expected = evaluate(*check.expected, local_context);
          }
          /*
            At this point, expected_type and expected just contain their literal values.

            We wish to separate this into (optional) checks expected_type and expected_value
            to appropriately model the situation.

            Consumes every reference into the equations.
          */
          std::optional<solver::Equation> rhs_type_equation;
          std::optional<solver::Equation> type_equation;
          std::optional<solver::Equation> value_equation;
          if(expected) {
            if(expected_type) {
              rhs_type_equation = solver::Equation{
                .lhs = std::move(expected->type),
                .rhs = interface.arena.copy(*expected_type),
                .stack = local_context
              };
              type_equation = solver::Equation{
                .lhs = std::move(term.type),
                .rhs = std::move(*expected_type),
                .stack = local_context
              };
              value_equation = solver::Equation{
                .lhs = std::move(term.value),
                .rhs = std::move(expected->value),
                .stack = local_context
              };
            } else {
              type_equation = solver::Equation{
                .lhs = std::move(term.type),
                .rhs = std::move(expected->type),
                .stack = local_context
              };
              value_equation = solver::Equation{
                .lhs = std::move(term.value),
                .rhs = std::move(expected->value),
                .stack = local_context
              };
            }
          } else {
            if(expected_type) {
              type_equation = solver::Equation{
                .lhs = std::move(term.type),
                .rhs = std::move(*expected_type),
                .stack = local_context
              };
              destroy_from_arena(interface.arena, term.value);
            } else {
              //nothing to check, I guess
              destroy_from_arena(interface.arena, term);
              return;
            }
          }
          //type_equation must be set. rhs_type_equation might be set. value_equation might be set.
          struct SharedState {
            mdb::Promise<bool> finished;
            std::uint8_t remaining;
            SharedState(mdb::Promise<bool> finished, bool rhs_equation):finished(std::move(finished)), remaining(rhs_equation ? 2 : 1) {}
            void mark_solved() {
              if(--remaining == 0) {
                finished.set_value(true);
              }
            }
            ~SharedState() {
              if(remaining > 0) {
                finished.set_value(false);
              }
            }
          };
          auto [type_promise, type_future] = mdb::create_promise_future_pair<bool>();
          {
            auto shared = std::make_shared<SharedState>(std::move(type_promise), rhs_type_equation.has_value());
            solver(std::move(*type_equation)).listen([this, shared, &check](EquationResult result) {
              if(std::holds_alternative<EquationSolved>(result)) {
                shared->mark_solved();
              } else {
                call_on_error(std::move(result), [this, &check](EquationErrorInfo error_info) {
                  interface.report_error(error::FailedTypeRequirement{
                    .allow_deduction = check.allow_deduction,
                    .check = check.index(),
                    .equation = std::move(error_info)
                  });
                });
              }
            });
            if(rhs_type_equation) {
              solver(std::move(*rhs_type_equation)).listen([this, shared, &check](EquationResult result) {
                if(std::holds_alternative<EquationSolved>(result)) {
                  shared->mark_solved();
                } else {
                  call_on_error(std::move(result), [this, &check](EquationErrorInfo error_info) {
                    interface.report_error(error::MismatchedRequirementRHSType {
                      .allow_deduction = check.allow_deduction,
                      .check = check.index(),
                      .equation = std::move(error_info)
                    });
                  });
                }
              });
            }
          }
          if(value_equation) {
            std::move(type_future).listen([this, &check, &solver, value_equation = std::move(*value_equation)](bool okay) mutable {
              if(!okay) {
                destroy_from_arena(interface.arena, value_equation);
                return;
              }
              solver(std::move(value_equation)).listen([this, &solver, &check](EquationResult result) {
                if(!std::holds_alternative<EquationSolved>(result)) {
                  call_on_error(std::move(result), [this, &check](EquationErrorInfo error_info) {
                    interface.report_error(error::FailedRequirement{
                      .allow_deduction = check.allow_deduction,
                      .check = check.index(),
                      .equation = std::move(error_info)
                    });
                  });
                }
              });
            });
          }
        },
        [&](instruction_archive::Let const& let) {
          if(let.type) {
            auto value = evaluate(let.value, local_context);
            auto type = evaluate(*let.type, local_context);

            auto result = cast_typed(
              std::move(value),
              cast(
                std::move(type),
                interface.arena.copy(interface.type),
                local_context,
                variable_explanation::LetTypeCast{let.index()},
                error_listener<error::BadLetType>(let.index())
              ),
              local_context,
              variable_explanation::LetCast{let.index()},
              error_listener<error::MismatchedLetType>(let.index())
            );
            locals.push_back(std::move(result));
          } else {
            auto result = evaluate(let.value, local_context);
            locals.push_back(std::move(result));
          }
        }
      });
    }
  };
  TypedValue evaluate(compiler::new_instruction::output::archive_part::ProgramRoot const& root, EvaluatorInterface interface) {
    auto local_context = Stack::empty({
      .type = interface.type,
      .arrow = interface.arrow,
      .id = interface.id,
      .constant = interface.constant,
      .type_family = interface.type_family,
      .arena = interface.arena,
      .rule_collector = interface.rule_collector,
      .type_collector = interface.type_collector,
      .register_type = interface.register_type,
      .register_declaration = interface.register_declaration,
      .add_rule = interface.add_rule
    });
    ExpressionEvaluateDetail detail{
      .interface = std::move(interface)
    };
    for(auto const& command : root.commands) {
      detail.evaluate(command, local_context);
    }
    auto ret = detail.evaluate(root.value, local_context);
    for(auto& local : detail.locals) {
      destroy_from_arena(detail.interface.arena, local);
    }
    detail.interface.close_interface();
    return ret;
  }
}
