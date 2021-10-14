#include "evaluator.hpp"
#include "../NewExpression/arena_utility.hpp"

namespace solver::evaluator {
  using TypedValue = new_expression::TypedValue;
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Stack = stack::Stack;
  namespace instruction_archive = compiler::instruction::output::archive_part;
  using Expression = expression::tree::Expression;
  struct ExpressionEvaluateDetail {
    EvaluatorInterface interface;
    std::vector<TypedValue> locals;

    TypedValue make_variable_typed(OwnedExpression type, Stack& local_context, bool definable) {
      //auto true_type = local_context.instance_of_type_family(interface.arena.copy(type));
      auto var = interface.arena.declaration();
      interface.register_declaration(var);
      if(definable) {
        interface.register_definable_indeterminate(interface.arena.copy(var));
      }
      return {.value = local_context.apply_args(std::move(var)), .type = std::move(type)};
    }
    OwnedExpression make_variable(OwnedExpression type, Stack& local_context, bool definable) {
      auto typed = make_variable_typed(std::move(type), local_context, definable);
      interface.arena.drop(std::move(typed.type));
      return std::move(typed.value);
    }
    TypedValue cast_typed(TypedValue input, OwnedExpression new_type, Stack& local_context) {
      input.type = interface.reduce(std::move(input.type));
      new_type = interface.reduce(std::move(new_type));
      if(input.type == new_type) {
        interface.arena.drop(std::move(new_type));
        return std::move(input);
      }
      auto cast_var = make_variable(interface.arena.copy(new_type), local_context, false);
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
    OwnedExpression cast(TypedValue input, OwnedExpression new_type, Stack& local_context) {
      auto typed = cast_typed(std::move(input), std::move(new_type), local_context);
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
            lhs.type = interface.reduce(std::move(lhs.type));
            auto lhs_unfolded = unfold(interface.arena, lhs.type);
            if(lhs_unfolded.head == interface.arrow && lhs_unfolded.args.size() == 2) {
              new_expression::RAIIDestroyer destroyer{interface.arena, lhs.type};
              return std::make_tuple(
                std::move(lhs.value),
                interface.arena.copy(lhs_unfolded.args[1]),
                cast(
                  std::move(rhs),
                  interface.arena.copy(lhs_unfolded.args[0]),
                  local_context
                )
              );
            }
            auto domain = make_variable(
              interface.arena.copy(interface.type),
              local_context,
              true
            );
            auto codomain = make_variable(
              interface.arena.apply(
                interface.arena.copy(interface.type_family),
                interface.arena.copy(domain)
              ),
              local_context,
              true
            );
            auto expected_function_type = interface.arena.apply(
              interface.arena.copy(interface.arrow),
              interface.arena.copy(domain),
              interface.arena.copy(codomain)
            );
            auto func = make_variable(interface.arena.copy(expected_function_type), local_context, false);
            auto arg = make_variable(interface.arena.copy(domain), local_context, false);
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
            case compiler::instruction::Primitive::type: return TypedValue{
              .value = interface.arena.copy(interface.type),
              .type = interface.arena.copy(interface.type)
            };
            case compiler::instruction::Primitive::arrow: return {
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
            local_context
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
    void create_declaration(instruction_archive::Expression const& type, Stack& local_context, bool definable) {
      auto type_eval = evaluate(type, local_context);
      auto value = make_variable_typed(
        cast(
          std::move(type_eval),
          interface.arena.copy(interface.type),
          local_context
        ),
        local_context,
        definable
      );
      locals.push_back(std::move(value));
    }
    void evaluate(instruction_archive::Command const& command, Stack& local_context) {
      return command.visit(mdb::overloaded{
        [&](instruction_archive::DeclareHole const& hole) {
          return create_declaration(hole.type, local_context, true);
        },
        [&](instruction_archive::Declare const& declare) {
          return create_declaration(declare.type, local_context, false);
        },
        [&](instruction_archive::Axiom const& axiom) {
          return create_declaration(axiom.type, local_context, false);
        },
        [&](instruction_archive::Rule const& rule) {
          auto pattern = evaluate(rule.pattern, local_context);
          auto replacement = evaluate(rule.replacement, local_context);
          interface.rule({
            .stack = local_context,
            .pattern_type = std::move(pattern.type),
            .pattern = std::move(pattern.value),
            .replacement_type = std::move(replacement.type),
            .replacement = std::move(replacement.value)
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
                local_context
              ),
              local_context
            );
            locals.push_back(std::move(result));
          } else {
            auto result = evaluate(let.value, local_context);
            locals.push_back(std::move(result));
          }
        },
        [&](instruction_archive::ForAll const& for_all) {
          auto local_size = locals.size();
          auto local_type_raw = evaluate(for_all.type, local_context);
          auto local_type = cast(
            std::move(local_type_raw),
            interface.arena.copy(interface.type),
            local_context
          );
          locals.push_back({
            .value = interface.arena.argument(local_context.depth()),
            .type = interface.arena.copy(local_type)
          });
          auto new_context = local_context.extend(std::move(local_type));
          for(auto const& command : for_all.commands) {
            evaluate(command, new_context);
          }
          for(auto it = locals.begin() + local_size; it != locals.end(); ++it) {
            destroy_from_arena(interface.arena, *it); //destroy typedvalues
          }
          locals.erase(locals.begin() + local_size, locals.end()); //restore stack
        }
      });
    }
  };
  TypedValue evaluate(compiler::instruction::output::archive_part::ProgramRoot const& root, EvaluatorInterface interface) {
    auto local_context = Stack::empty({
      .type = interface.type,
      .arrow = interface.arrow,
      .id = interface.id,
      .arena = interface.arena,
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
