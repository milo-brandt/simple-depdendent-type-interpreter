#include "evaluator.hpp"
#include "../Expression/evaluation_context.hpp"

namespace compiler::evaluate {
  namespace instruction_archive = instruction::output::archive_part;
  using Expression = expression::tree::Expression;
  EvaluateResult evaluate_tree(instruction::output::archive_part::ProgramRoot const& root, expression::Context& expression_context, mdb::function<expression::TypedValue(std::uint64_t)> embed) {
    struct Detail {
      expression::Context& expression_context;
      mdb::function<expression::TypedValue(std::uint64_t)> embed;
      std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
      std::vector<Cast> casts;
      std::vector<Rule> rules;
      std::vector<expression::TypedValue> locals;

      expression::TypedValue make_variable_typed(expression::tree::Expression type, variable_explanation::Any explanation, expression::Stack& local_context) {
        auto true_type = local_context.instance_of_type_family(expression_context, type);
        auto var = expression_context.create_variable({
          .is_axiom = variable_explanation::is_axiom(explanation),
          .type = true_type
        });
        variables.insert(std::make_pair(var, std::move(explanation)));
        return {.value = local_context.apply_args(expression::tree::External{var}), .type = std::move(type)};
      }
      expression::tree::Expression make_variable(expression::tree::Expression type, variable_explanation::Any explanation, expression::Stack& local_context) {
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
      expression::tree::Expression cast(expression::TypedValue input, expression::tree::Expression new_type, variable_explanation::Any cast_var_explanation, expression::Stack& local_context) {
        if(expression_context.reduce(input.type) == expression_context.reduce(new_type)) return std::move(input.value);
        auto cast_var = make_variable(new_type, cast_var_explanation, local_context);
        auto var = expression::unfold_ref(cast_var).head->get_external().external_index;
        casts.push_back({
          .stack = local_context,
          .variable = var,
          .source_type = std::move(input.type),
          .source = std::move(input.value),
          .target_type = std::move(new_type)
        });
        return cast_var;
      }
      expression::TypedValue cast_typed(expression::TypedValue input, expression::tree::Expression new_type, variable_explanation::Any cast_var_explanation, expression::Stack& local_context) {
        auto new_value = cast(std::move(input), new_type, cast_var_explanation, local_context);
        return {
          .value = std::move(new_value),
          .type = std::move(new_type)
        };
      }
      expression::TypedValue evaluate(instruction_archive::Expression const& tree, expression::Stack& local_context) {
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
                          variable_explanation::ApplyRHSCast{local_context.depth(), apply.index()},
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
                variable_explanation::ApplyCodomain{local_context.depth(), apply.index()},
                local_context
              );
              auto casted = cast_typed(
                std::move(lhs),
                arrow_type(rhs.type, codomain),
                variable_explanation::ApplyLHSCast{local_context.depth(), apply.index()},
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
              variable_explanation::TypeFamilyCast{local_context.depth(), type_family.index()},
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
      void evaluate(instruction_archive::Command const& command, expression::Stack& local_context) {
        command.visit(mdb::overloaded{
          [&](instruction_archive::DeclareHole const& hole) {
            locals.push_back(make_variable_typed(
              cast(
                evaluate(hole.type, local_context),
                expression::tree::External{expression_context.primitives.type},
                variable_explanation::HoleTypeCast{local_context.depth(), hole.index()},
                local_context
              ), variable_explanation::ExplicitHole{local_context.depth(), hole.index()}, local_context)
            );
          },
          [&](instruction_archive::Declare const& declare) {
            locals.push_back(make_variable_typed(
              cast(
                evaluate(declare.type, local_context),
                expression::tree::External{expression_context.primitives.type},
                variable_explanation::DeclareTypeCast{local_context.depth(), declare.index()},
                local_context
              ), variable_explanation::Declaration{local_context.depth(), declare.index()}, local_context)
            );
          },
          [&](instruction_archive::Axiom const& axiom) {
            locals.push_back(make_variable_typed(
              cast(
                evaluate(axiom.type, local_context),
                expression::tree::External{expression_context.primitives.type},
                variable_explanation::AxiomTypeCast{local_context.depth(), axiom.index()},
                local_context
              ), variable_explanation::Axiom{local_context.depth(), axiom.index()}, local_context)
            );
          },
          [&](instruction_archive::Rule const& rule) {
            auto pattern = evaluate(rule.pattern, local_context);
            auto replacement = evaluate(rule.replacement, local_context);
            rules.push_back({
              .stack = local_context,
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
                  variable_explanation::LetTypeCast{local_context.depth(), let.index()},
                  local_context
                ),
                variable_explanation::LetCast{local_context.depth(), let.index()},
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
              variable_explanation::ForAllTypeCast{local_context.depth(), for_all.index()},
              local_context
            );
            locals.push_back({
              .value = expression::tree::Arg{ .arg_index = local_context.depth() },
              .type = local_type
            });
            auto new_context = local_context.extend(expression_context, local_type);
            for(auto const& command : for_all.commands) {
              evaluate(command, new_context);
            }
            locals.erase(locals.begin() + local_size, locals.end()); //restore stack
          }
        });
      }
    };
    auto local_context = expression::Stack::empty(expression_context);
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
  PatternEvaluateResult evaluate_pattern(pattern::archive_part::Pattern const& pattern, expression::Context& expression_context) {
    struct Detail {
      expression::Context& expression_context;
      std::unordered_map<std::uint64_t, pattern_variable_explanation::Any> variables;
      std::vector<Cast> casts;
      std::vector<std::uint64_t> capture_point_variables;
      expression::TypedValue evaluate_pattern(pattern::archive_part::Pattern const& pat) {
        return pat.visit(mdb::overloaded{
          [&](pattern::archive_part::CapturePoint const& point) -> expression::TypedValue {
            auto type_var = expression_context.create_variable({
              .is_axiom = false,
              .type = expression::tree::External{expression_context.primitives.type}
            });
            auto var = expression_context.create_variable({
              .is_axiom = false,
              .type = expression::tree::External{type_var}
            });

            capture_point_variables.push_back(var);
            variables.insert(std::make_pair(var, pattern_variable_explanation::CapturePoint{ point.index() }));
            variables.insert(std::make_pair(type_var, pattern_variable_explanation::CapturePointType{ point.index() }));

            return {
              .value = expression::tree::External{var},
              .type = expression::tree::External{type_var}
            };
          },
          [&](pattern::archive_part::Segment const& segment) -> expression::TypedValue {
            expression::TypedValue head = expression_context.get_external(segment.head);
            std::uint64_t count = 0;
            for(auto const& arg : segment.args) {
              auto val = evaluate_pattern(arg);
              head.type = expression_context.reduce(std::move(head.type));
              if(auto* lhs_apply = head.type.get_if_apply()) {
                if(auto* inner_apply = lhs_apply->lhs.get_if_apply()) {
                  if(auto* lhs_ext = inner_apply->lhs.get_if_external()) {
                    if(lhs_ext->external_index == expression_context.primitives.arrow) {
                      val.type = expression_context.reduce(std::move(val.type));
                      if(val.type != inner_apply->rhs) {
                        auto cast_var = expression_context.create_variable({
                          .is_axiom = false,
                          .type = inner_apply->rhs
                        });
                        variables.insert(std::make_pair(cast_var, pattern_variable_explanation::ApplyCast{ count, segment.index() }));
                        casts.push_back({
                          .stack = expression::Stack::empty(expression_context),
                          .variable = cast_var,
                          .source_type = val.type,
                          .source = val.value,
                          .target_type = inner_apply->rhs
                        });
                        head = expression::TypedValue{
                          .value = expression::tree::Apply{std::move(head.value), expression::tree::External{cast_var}},
                          .type = expression::tree::Apply{std::move(lhs_apply->rhs), expression::tree::External{cast_var}}
                        };
                      } else {
                        head = expression::TypedValue{
                          .value = expression::tree::Apply{std::move(head.value), val.value},
                          .type = expression::tree::Apply{std::move(lhs_apply->rhs), val.value}
                        };
                      }
                      ++count;
                      continue;
                    }
                  }
                }
              }
              std::terminate(); //o no panic
            }
            return head;
          }
        });
      }
    };
    Detail detail{
      .expression_context = expression_context
    };
    detail.evaluate_pattern(pattern);
    return {
      .variables = std::move(detail.variables),
      .casts = std::move(detail.casts),
      .capture_point_variables = std::move(detail.capture_point_variables)
    };
  }
}
