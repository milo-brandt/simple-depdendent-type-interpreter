#include "evaluator.hpp"
#include "../NewExpression/arena_utility.hpp"
#include "rule_utility.hpp"

namespace solver::evaluator {
  using TypedValue = new_expression::TypedValue;
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Stack = stack::Stack;
  namespace instruction_archive = compiler::new_instruction::output::archive_part;
  using Expression = expression::tree::Expression;
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
        interface.register_definable_indeterminate(interface.arena.copy(var));
      }
      return {.value = local_context.apply_args(std::move(var)), .type = std::move(type)};
    }
    template<VariableKind kind>
    OwnedExpression make_variable(OwnedExpression type, Stack& local_context, variable_explanation::Any explanation) {
      auto typed = make_variable_typed<kind>(std::move(type), local_context, std::move(explanation));
      interface.arena.drop(std::move(typed.type));
      return std::move(typed.value);
    }
    TypedValue cast_typed(TypedValue input, OwnedExpression new_type, Stack& local_context, variable_explanation::Any cast_var_explanation) {
      input.type = local_context.reduce(std::move(input.type));
      new_type = local_context.reduce(std::move(new_type));
      if(input.type == new_type) {
        interface.arena.drop(std::move(new_type));
        return std::move(input);
      }
      auto cast_var = make_variable<VariableKind::declaration>(interface.arena.copy(new_type), local_context, std::move(cast_var_explanation));
      auto var = new_expression::unfold(interface.arena, cast_var).head;
      interface.cast({
        .stack = local_context,
        .variable = interface.arena.copy(var),
        .source_type = std::move(input.type),
        .source = std::move(input.value),
        .target_type = interface.arena.copy(new_type)
      });
      return {
        .value = std::move(cast_var),
        .type = std::move(new_type)
      };
    }
    OwnedExpression cast(TypedValue input, OwnedExpression new_type, Stack& local_context, variable_explanation::Any cast_var_explanation) {
      auto typed = cast_typed(std::move(input), std::move(new_type), local_context, std::move(cast_var_explanation));
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
                  variable_explanation::ApplyRHSCast{apply.index()}
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
            interface.function_cast({
              .stack = local_context,
              .function_variable = interface.arena.copy(func_var),
              .argument_variable = interface.arena.copy(arg_var),
              .function_value = std::move(lhs.value),
              .function_type = std::move(lhs.type),
              .expected_function_type = std::move(expected_function_type),
              .argument_value = std::move(rhs.value),
              .argument_type = std::move(rhs.type),
              .expected_argument_type = std::move(domain)
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
            variable_explanation::TypeFamilyCast{type_family.index()}
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
    template<VariableKind kind>
    void create_declaration(instruction_archive::Expression const& type, Stack& local_context, variable_explanation::Any cast_explanation, variable_explanation::Any var_explanation) {
      auto type_eval = evaluate(type, local_context);
      auto value = make_variable_typed<kind>(
        cast(
          std::move(type_eval),
          interface.arena.copy(interface.type),
          local_context,
          std::move(cast_explanation)
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
            variable_explanation::ExplicitHole{hole.index()}
          );
        },
        [&](instruction_archive::Declare const& declare) {
          return create_declaration<VariableKind::declaration>(
            declare.type,
            local_context,
            variable_explanation::DeclareTypeCast{declare.index()},
            variable_explanation::Declaration{declare.index()}
          );
        },
        [&](instruction_archive::Axiom const& axiom) {
          return create_declaration<VariableKind::axiom>(
            axiom.type,
            local_context,
            variable_explanation::AxiomTypeCast{axiom.index()},
            variable_explanation::Axiom{axiom.index()}
          );
        },
        [&](instruction_archive::Rule const& rule) {
          //if(rule.secondary_matches.begin() != rule.secondary_matches.end()) std::terminate();
          //if(rule.secondary_patterns.begin() != rule.secondary_patterns.end()) std::terminate();
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
          auto resolved = resolve_pattern(rule.primary_pattern, resolve_interface);
          std::vector<RawPatternShard> subpatterns;
          for(auto const& submatch_generic : rule.submatches) {
            auto const& submatch = submatch_generic.get_submatch();
            subpatterns.push_back({
              .used_captures = submatch.captures_used,
              .pattern = resolve_pattern(submatch.pattern, resolve_interface)
            });
          }
          auto folded = normalize_pattern(interface.arena, RawPattern{
            .primary_pattern = std::move(resolved),
            .subpatterns = std::move(subpatterns)
          }, rule.capture_count);
          auto flat = flatten_pattern(interface.arena, std::move(folded));
          auto executed = execute_pattern({
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
          std::vector<std::pair<OwnedExpression, OwnedExpression> > checks = std::move(executed.checks);
          checks.emplace_back(std::move(executed.type_of_pattern), std::move(replacement.type));
          interface.rule({
            .stack = executed.pattern_stack,
            .rule = std::move(proposed_rule),
            .checks = std::move(checks)
          });
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
                variable_explanation::LetTypeCast{let.index()}
              ),
              local_context,
              variable_explanation::LetCast{let.index()}
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
    for(auto& local : detail.locals) {
      destroy_from_arena(detail.interface.arena, local);
    }
    return detail.evaluate(root.value, local_context);
  }
}
