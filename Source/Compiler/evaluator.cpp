#include "evaluator.hpp"
#include "../Expression/evaluation_context.hpp"

namespace compiler::evaluate {
  namespace instruction_archive = instruction::output::archive_part;
  using Expression = expression::tree::Expression;
  namespace {
    struct ContextInfo {
      std::uint64_t depth;
      Expression fam; //Type of type families; e.g. (x : Nat) -> (y : IsPrime x) -> Type
      Expression var; //Embedding of type families; e.g. \f : fam.(x : Nat) -> (y : IsPrime x) -> f x y
    };
    ContextInfo base_context(expression::Context& context) {
      return {
        .depth = 0,
        .fam = expression::tree::External{context.primitives.type},
        .var = expression::tree::External{context.primitives.id}
      };
    }
    ContextInfo extend_context(ContextInfo const& context, expression::tree::Expression base_family, expression::Context& expression_context) {
      return expression_context.create_variables<4>([&](auto&& build, auto inner_constant_family, auto family_over, auto as_fibration, auto var_p) {
        Expression new_fam = expression::tree::Apply{
          context.var,
          expression::tree::External{family_over}
        };
        build(expression::ExternalInfo{ //inner_constant_family
          .is_axiom = false,
          .type = new_fam
        }, expression::ExternalInfo{ //family_over
          .is_axiom = false,
          .type = context.fam
        }, expression::ExternalInfo{ //as_fibration
          .is_axiom = false,
          .type = expression::multi_apply(
            expression::tree::External{expression_context.primitives.arrow},
            new_fam,
            expression::multi_apply(
              expression::tree::External{expression_context.primitives.constant},
              expression::tree::External{expression_context.primitives.type},
              expression::tree::External{expression_context.primitives.type},
              new_fam
            )
          )
        }, expression::ExternalInfo{ //var_p
          .is_axiom = false,
          .type = expression::tree::Apply{
            expression::tree::External{expression_context.primitives.type_family},
            new_fam
          }
        });
        auto apply_args = [](Expression head, std::uint64_t arg_start, std::uint64_t arg_count) {
          for(std::uint64_t i = 0; i < arg_count; ++i) {
            head = expression::tree::Apply{std::move(head), expression::tree::Arg{arg_start + i}};
          }
          return head;
        };
        expression_context.rules.push_back({
          .pattern = expression::lambda_pattern(inner_constant_family, context.depth + 1),
          .replacement = expression::tree::External{expression_context.primitives.type}
        });
        expression_context.rules.push_back({
          .pattern = expression::lambda_pattern(family_over, context.depth),
          .replacement = expression::multi_apply(
            expression::tree::External{expression_context.primitives.arrow},
            apply_args(base_family, 0, context.depth),
            apply_args(expression::tree::External{inner_constant_family}, 0, context.depth)
          )
        });
        expression_context.rules.push_back({
          .pattern = expression::lambda_pattern(as_fibration, context.depth + 1),
          .replacement = expression::multi_apply(
            expression::tree::External{expression_context.primitives.arrow},
            apply_args(base_family, 1, context.depth),
            apply_args(expression::tree::Arg{0}, 1, context.depth)
          )
        });
        expression_context.rules.push_back({
          .pattern = expression::lambda_pattern(var_p, 1),
          .replacement = expression::tree::Apply{
            context.var,
            expression::tree::Apply{
              expression::tree::External{as_fibration},
              expression::tree::Arg{0}
            }
          }
        });
        return ContextInfo{
          .depth = context.depth + 1,
          .fam = std::move(new_fam),
          .var = expression::tree::External{var_p}
        };
      });
    }
  }
  EvaluateResult evaluate_tree(instruction::output::archive_part::ProgramRoot const& root, expression::Context& expression_context, mdb::function<expression::TypedValue(std::uint64_t)> embed) {
    struct Detail {
      expression::Context& expression_context;
      mdb::function<expression::TypedValue(std::uint64_t)> embed;
      std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
      std::vector<Cast> casts;
      std::vector<Rule> rules;
      std::vector<expression::TypedValue> locals;

      expression::TypedValue make_variable_typed(expression::tree::Expression type, variable_explanation::Any explanation, ContextInfo const& local_context) {
        auto type_var = expression_context.create_variable({
          .is_axiom = false,
          .type = local_context.fam
        });
        expression_context.rules.push_back({
          .pattern = expression::lambda_pattern(type_var, local_context.depth),
          .replacement = type
        });
        auto true_type = expression::tree::Apply{
          local_context.var,
          expression::tree::External{type_var}
        };
        auto var = expression_context.create_variable({
          .is_axiom = variable_explanation::is_axiom(explanation),
          .type = true_type
        });
        variable_explanation::VarType var_type{
          local_context.depth,
          var,
          std::visit([&](auto const& expl) -> instruction::archive_index::PolymorphicKind { return expl.index; }, explanation)
        };
        variables.insert(std::make_pair(var, std::move(explanation)));
        variables.insert(std::make_pair(type_var, std::move(var_type)));
        expression::tree::Expression ret = expression::tree::External{var};
        for(std::size_t i = 0; i < local_context.depth; ++i) {
          ret = expression::tree::Apply{std::move(ret), expression::tree::Arg{i}};
        }
        return {.value = std::move(ret), .type = std::move(type)};
      }
      expression::tree::Expression make_variable(expression::tree::Expression type, variable_explanation::Any explanation, ContextInfo const& local_context) {
        return make_variable_typed(std::move(type), std::move(explanation), local_context).value;
      }
      expression::tree::Expression arrow_type(expression::tree::Expression domain, expression::tree::Expression codomain) {
        return expression::tree::Apply{
          expression::tree::Apply{
            expression::tree::External{expression_context.primitives.arrow},
            std::move(domain)
          },
          std::move(codomain)
        };
      }
      expression::tree::Expression cast(expression::TypedValue input, expression::tree::Expression new_type, variable_explanation::Any cast_var_explanation, ContextInfo const& local_context) {
        if(expression_context.reduce(input.type) == expression_context.reduce(new_type)) return std::move(input.value);
        auto cast_var = make_variable(new_type, cast_var_explanation, local_context);
        auto var = expression::unfold_ref(cast_var).head->get_external().external_index;
        casts.push_back({
          .depth = local_context.depth,
          .variable = var,
          .source_type = std::move(input.type),
          .source = std::move(input.value),
          .target_type = std::move(new_type)
        });
        return cast_var;
      }
      expression::TypedValue cast_typed(expression::TypedValue input, expression::tree::Expression new_type, variable_explanation::Any cast_var_explanation, ContextInfo const& local_context) {
        auto new_value = cast(std::move(input), new_type, cast_var_explanation, local_context);
        return {
          .value = std::move(new_value),
          .type = std::move(new_type)
        };
      }
      expression::TypedValue evaluate(instruction_archive::Expression const& tree, ContextInfo const& local_context) {
        return tree.visit(mdb::overloaded{
          [&](instruction_archive::Apply const& apply) -> expression::TypedValue {
            auto lhs = evaluate(apply.lhs, local_context);
            auto rhs = evaluate(apply.rhs, local_context);
            auto [lhs_value, lhs_domain, lhs_codomain, rhs_value] = [&]() -> std::tuple<expression::tree::Expression, expression::tree::Expression, expression::tree::Expression, expression::tree::Expression> {
              auto lhs_type = expression_context.reduce(lhs.type);
              if(auto* lhs_apply = lhs_type.get_if_apply()) {
                if(auto* inner_apply = lhs_apply->lhs.get_if_apply()) {
                  if(auto* lhs_ext = inner_apply->lhs.get_if_external()) {
                    if(lhs_ext->external_index == expression_context.primitives.arrow) {
                      auto domain = inner_apply->rhs;
                      return std::make_tuple(
                        std::move(lhs.value),
                        std::move(inner_apply->rhs),
                        std::move(lhs_apply->rhs),
                        cast(
                          std::move(rhs),
                          std::move(domain),
                          variable_explanation::ApplyRHSCast{local_context.depth, apply.index()},
                          local_context
                        )
                      );
                    }
                  }
                }
              }
              auto codomain = make_variable(
                expression::tree::Apply{
                  expression::tree::External{expression_context.primitives.type_family},
                  rhs.type
                },
                variable_explanation::ApplyCodomain{local_context.depth, apply.index()},
                local_context
              );
              auto casted = cast_typed(
                std::move(lhs),
                arrow_type(rhs.type, codomain),
                variable_explanation::ApplyLHSCast{local_context.depth, apply.index()},
                local_context
              );
              return std::make_tuple(
                std::move(casted.value),
                std::move(casted.type),
                std::move(codomain),
                std::move(rhs.value)
              );
            } ();
            return expression::TypedValue{
              .value = expression::tree::Apply{lhs_value, rhs_value},
              .type = expression::tree::Apply{lhs_codomain, rhs_value}
            };
          },
          [&](instruction_archive::Local const& local) -> expression::TypedValue {
            return locals.at(local.local_index);
          },
          [&](instruction_archive::Embed const& e) -> expression::TypedValue {
            return embed(e.embed_index);
          },
          [&](instruction_archive::PrimitiveExpression const& primitive) -> expression::TypedValue {
            switch(primitive.primitive) {
              case instruction::Primitive::type: return expression_context.get_external(expression_context.primitives.type);
              case instruction::Primitive::arrow: return expression_context.get_external(expression_context.primitives.arrow);
            }
            std::terminate();
          },
          [&](instruction_archive::TypeFamilyOver const& type_family) -> expression::TypedValue {
            auto type = cast(
              evaluate(type_family.type, local_context),
              expression::tree::External{expression_context.primitives.type},
              variable_explanation::TypeFamilyCast{local_context.depth, type_family.index()},
              local_context
            );
            return {
              .value = expression::tree::Apply{
                expression::tree::External{expression_context.primitives.type_family},
                std::move(type)
              },
              .type = expression::tree::External{expression_context.primitives.type}
            };
          }
        });
      }
      void evaluate(instruction_archive::Command const& command, ContextInfo const& local_context) {
        command.visit(mdb::overloaded{
          [&](instruction_archive::DeclareHole const& hole) {
            locals.push_back(make_variable_typed(
              cast(
                evaluate(hole.type, local_context),
                expression::tree::External{expression_context.primitives.type},
                variable_explanation::HoleTypeCast{local_context.depth, hole.index()},
                local_context
              ), variable_explanation::ExplicitHole{local_context.depth, hole.index()}, local_context)
            );
          },
          [&](instruction_archive::Declare const& declare) {
            locals.push_back(make_variable_typed(
              cast(
                evaluate(declare.type, local_context),
                expression::tree::External{expression_context.primitives.type},
                variable_explanation::DeclareTypeCast{local_context.depth, declare.index()},
                local_context
              ), variable_explanation::Declaration{local_context.depth, declare.index()}, local_context)
            );
          },
          [&](instruction_archive::Axiom const& axiom) {
            locals.push_back(make_variable_typed(
              cast(
                evaluate(axiom.type, local_context),
                expression::tree::External{expression_context.primitives.type},
                variable_explanation::AxiomTypeCast{local_context.depth, axiom.index()},
                local_context
              ), variable_explanation::Axiom{local_context.depth, axiom.index()}, local_context)
            );
          },
          [&](instruction_archive::Rule const& rule) {
            auto pattern = evaluate(rule.pattern, local_context);
            auto replacement = evaluate(rule.replacement, local_context);
            rules.push_back({
              .depth = local_context.depth,
              .pattern_type = std::move(pattern.type),
              .pattern = std::move(pattern.value),
              .replacement_type = std::move(replacement.type),
              .replacement = std::move(replacement.value)
            });
          },
          [&](instruction_archive::Let const& let) {
            if(let.type) {
              locals.push_back(cast_typed(
                evaluate(let.value, local_context),
                cast(
                  evaluate(*let.type, local_context),
                  expression::tree::External{expression_context.primitives.type},
                  variable_explanation::LetTypeCast{local_context.depth, let.index()},
                  local_context
                ),
                variable_explanation::LetCast{local_context.depth, let.index()},
                local_context
              ));
            } else {
              locals.push_back(evaluate(let.value, local_context));
            }
          },
          [&](instruction_archive::ForAll const& for_all) {
            auto local_size = locals.size();
            auto local_type = cast(
              evaluate(for_all.type, local_context),
              expression::tree::External{expression_context.primitives.type},
              variable_explanation::ForAllTypeCast{local_context.depth, for_all.index()},
              local_context
            );
            locals.push_back({
              .value = expression::tree::Arg{ .arg_index = local_context.depth },
              .type = local_type
            });
            auto new_context = extend_context(local_context, local_type, expression_context);
            for(auto const& command : for_all.commands) {
              evaluate(command, new_context);
            }
            locals.erase(locals.begin() + local_size, locals.end()); //restore stack
          }
        });
      }
    };
    auto local_context = base_context(expression_context);
    Detail detail{
      .expression_context = expression_context,
      .embed = std::move(embed)
    };
    for(auto const& command : root.commands) {
      detail.evaluate(command, local_context);
    }
    auto result = detail.evaluate(root.value, local_context);
    return {
      .variables = std::move(detail.variables),
      .casts = std::move(detail.casts),
      .rules = std::move(detail.rules),
      .result = std::move(result)
    };
  }
}
