#include "evaluator.hpp"

namespace compiler::evaluate {
  namespace instruction_archive = instruction::output::archive_part;
  EvaluateResult evaluate_tree(instruction_archive::ProgramRoot const& root, EvaluateContext& context) {
    struct Detail {
      EvaluateContext& context;
      //std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
      std::vector<std::uint64_t> variables;
      std::vector<Cast> casts;
      std::vector<Rule> rules;
      std::vector<expression::TypedValue> locals;

      expression::tree::Expression make_variable(std::uint64_t depth, bool is_variable = true, bool is_axiom = false/*, variable_explanation::Any explanation*/) {
        auto var = context.allocate_variable(is_axiom);
        if(is_variable) variables.push_back(var);
        //variables.insert(std::make_pair(var, std::move(explanation)));
        expression::tree::Expression ret = expression::tree::External{var};
        for(std::size_t i = 0; i < depth; ++i) {
          ret = expression::tree::Apply{std::move(ret), expression::tree::Arg{i}};
        }
        return ret;
      }
      expression::tree::Expression arrow_type(expression::tree::Expression domain, expression::tree::Expression codomain) {
        return expression::tree::Apply{
          expression::tree::Apply{
            context.arrow_axiom.value,
            std::move(domain)
          },
          std::move(codomain)
        };
      }
      expression::tree::Expression cast(expression::TypedValue input, expression::tree::Expression new_type, std::uint64_t depth) {
        if(input.type == new_type) return std::move(input.value);
        auto cast_var = make_variable(depth, false);
        auto var = expression::unfold_ref(cast_var).head->get_external().external_index;
        casts.push_back({
          .depth = depth,
          .variable = var,
          .source_type = std::move(input.type),
          .source = std::move(input.value),
          .target_type = std::move(new_type)
        });
        return cast_var;
      }
      expression::TypedValue cast_typed(expression::TypedValue input, expression::tree::Expression new_type, std::uint64_t depth) {
        auto new_value = cast(std::move(input), new_type, depth);
        return {
          .value = std::move(new_value),
          .type = std::move(new_type)
        };
      }
      expression::TypedValue evaluate(instruction_archive::Expression const& tree, std::uint64_t depth) {
        return tree.visit(mdb::overloaded{
          [&](instruction_archive::Apply const& apply) -> expression::TypedValue {
            auto lhs = evaluate(apply.lhs, depth);
            auto rhs = evaluate(apply.rhs, depth);
            auto [lhs_value, lhs_domain, lhs_codomain, rhs_value] = [&]() -> std::tuple<expression::tree::Expression, expression::tree::Expression, expression::tree::Expression, expression::tree::Expression> {
              if(auto* lhs_apply = lhs.type.get_if_apply()) {
                if(auto* inner_apply = lhs_apply->lhs.get_if_apply()) {
                  if(inner_apply->lhs == context.arrow_axiom.value) {
                    auto domain = inner_apply->rhs;
                    return std::make_tuple(std::move(lhs.value), std::move(inner_apply->rhs), std::move(lhs_apply->rhs), cast(std::move(rhs), std::move(domain), depth));
                  }
                }
              }
              auto cast_value = make_variable(depth, false/*, variable_explanation::ApplyCastLHS{position}*/);
              auto cast_var = expression::unfold_ref(cast_value).head->get_external().external_index;

              auto domain = rhs.type;
              auto codomain = make_variable(depth/*, variable_explanation::ApplyCodomain{position}*/);
              casts.push_back({
                .depth = depth,
                .variable = cast_var,
                .source_type = std::move(lhs.type),
                .source = std::move(lhs.value),
                .target_type = arrow_type(domain, codomain)
              });
              return std::make_tuple(std::move(cast_value), std::move(domain), std::move(codomain), std::move(rhs.value));
            } ();
            return expression::TypedValue{
              .value = expression::tree::Apply{lhs_value, rhs_value},
              .type = expression::tree::Apply{lhs_codomain, rhs_value}
            };
          },
          [&](instruction_archive::Local const& local) -> expression::TypedValue {
            return locals.at(local.local_index);
          },
          [&](instruction_archive::Embed const& embed) -> expression::TypedValue {
            return context.embed(embed.embed_index);
          },
          [&](instruction_archive::PrimitiveExpression const& primitive) -> expression::TypedValue {
            switch(primitive.primitive) {
              case instruction::Primitive::type: return context.type_axiom;
              case instruction::Primitive::arrow: return context.arrow_axiom;
            }
            std::terminate();
          },
          [&](instruction_archive::TypeFamilyOver const& type_family) {
            return context.type_family_over(cast(evaluate(type_family.type, depth), context.type_axiom.value, depth));
          }
        });
      }
      void evaluate(instruction_archive::Command const& command, std::uint64_t depth) {
        command.visit(mdb::overloaded{
          [&](instruction_archive::DeclareHole const& hole) {
            locals.push_back(expression::TypedValue{
              .value = make_variable(depth),
              .type = cast(evaluate(hole.type, depth), context.type_axiom.value, depth)
            });
          },
          [&](instruction_archive::Declare const& declare) {
            locals.push_back(expression::TypedValue{
              .value = make_variable(depth, false),
              .type = cast(evaluate(declare.type, depth), context.type_axiom.value, depth)
            });
          },
          [&](instruction_archive::Axiom const& axiom) {
            locals.push_back(expression::TypedValue{
              .value = make_variable(depth, false, true),
              .type = cast(evaluate(axiom.type, depth), context.type_axiom.value, depth)
            });
          },
          [&](instruction_archive::Rule const& rule) {
            auto pattern = evaluate(rule.pattern, depth);
            auto replacement = evaluate(rule.replacement, depth);
            rules.push_back({
              .depth = depth,
              .pattern_type = std::move(pattern.type),
              .pattern = std::move(pattern.value),
              .replacement_type = std::move(replacement.type),
              .replacement = std::move(replacement.value)
            });
          },
          [&](instruction_archive::Let const& let) {
            if(let.type) {
              locals.push_back(cast_typed(
                evaluate(let.value, depth),
                cast(evaluate(*let.type, depth), context.type_axiom.value, depth),
                depth
              ));
            } else {
              locals.push_back(evaluate(let.value, depth));
            }
          },
          [&](instruction_archive::ForAll const& for_all) {
            auto local_size = locals.size();
            locals.push_back({
              .value = expression::tree::Arg{ .arg_index = depth },
              .type = cast(evaluate(for_all.type, depth), context.type_axiom.value, depth)
            });
            for(auto const& command : for_all.commands) {
              evaluate(command, depth + 1);
            }
            locals.erase(locals.begin() + local_size, locals.end()); //restore stack
          }
        });
      }
    };
    Detail detail{
      .context = context,
    };
    for(auto const& command : root.commands) {
      detail.evaluate(command, 0);
    }
    auto result = detail.evaluate(root.value, 0);
    return {
      .variables = std::move(detail.variables),
      .casts = std::move(detail.casts),
      .rules = std::move(detail.rules),
      .result = std::move(result)
    };
  }
}
