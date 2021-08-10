#include "evaluator.hpp"

namespace compiler::evaluate {
  EvaluateResult evaluate_tree(compiler::resolution::output::Tree const& tree, EvaluateContext& context) {
    auto get_function_parts_out = [function_pattern = expression::tree::match::Apply{
      expression::tree::match::Apply{
        expression::tree::full_match::External{expression::tree::match::Predicate{[arrow_axiom = context.arrow_axiom](std::uint64_t index){ return index == arrow_axiom; }}},
        expression::tree::match::Any{}
      },
      expression::tree::match::Any{}
    }](expression::tree::Tree const& tree) -> std::optional<std::pair<expression::tree::Tree, expression::tree::Tree> > {
      if(auto match = function_pattern.try_match(tree)) {
        return std::make_pair(match->lhs.rhs, match->rhs);
      } else {
        return std::nullopt;
      }
    };
    struct Detail {
      EvaluateContext& context;
      decltype(get_function_parts_out) get_function_parts;
      std::unordered_map<std::uint64_t, variable_explanation::Any> variables;
      std::vector<Cast> casts;
      std::vector<Rule> rules;
      std::vector<expression::TypedValue> locals;

      resolution::path::Path position;

      expression::tree::Tree make_variable(std::uint64_t depth, variable_explanation::Any explanation) {
        auto var = context.allocate_variable();
        variables.insert(std::make_pair(var, std::move(explanation)));
        expression::tree::Tree ret = expression::tree::External{var};
        for(std::size_t i = 0; i < depth; ++i) {
          ret = expression::tree::Apply{std::move(ret), expression::tree::Arg{i}};
        }
        return ret;
      }
      expression::tree::Tree arrow_type(expression::tree::Tree domain, expression::tree::Tree codomain) {
        return expression::tree::Apply{
          expression::tree::Apply{
            expression::tree::External{context.arrow_axiom},
            std::move(domain)
          },
          std::move(codomain)
        };
      }
      expression::TypedValue evaluate(resolution::output::Tree const& tree, std::uint64_t depth) {
        return tree.visit(mdb::overloaded{
          [&](resolution::output::Apply const& apply) {
            position.steps.push_back(resolution::output::path_segment_of(&resolution::output::Apply::lhs));
            auto lhs = evaluate(apply.lhs, depth);
            position.steps.back() = resolution::output::path_segment_of(&resolution::output::Apply::rhs);
            auto rhs = evaluate(apply.rhs, depth);
            position.steps.pop_back();


            auto lhs_value = make_variable(depth, variable_explanation::ApplyCastLHS{position});
            auto lhs_domain = make_variable(depth, variable_explanation::ApplyDomain{position});
            auto lhs_codomain = make_variable(depth, variable_explanation::ApplyCodomain{position});
            casts.push_back({
              .depth = depth,
              .source_type = std::move(lhs.type),
              .source = std::move(lhs.value),
              .target_type = arrow_type(lhs_domain, lhs_codomain),
              .target = lhs_value
            });
            auto rhs_value = make_variable(depth, variable_explanation::ApplyCastRHS{position});
            casts.push_back({
              .depth = depth,
              .source_type = std::move(rhs.type),
              .source = std::move(rhs.value),
              .target_type = lhs_domain,
              .target = rhs_value
            });
            return expression::TypedValue{
              .value = expression::tree::Apply{lhs_value, rhs_value},
              .type = expression::tree::Apply{lhs_codomain, rhs_value}
            };
          },
          [&](resolution::output::Lambda const& lambda) {
            auto lambda_domain = make_variable(depth, variable_explanation::LambdaCastDomain{position});

            position.steps.push_back(resolution::output::path_segment_of(&resolution::output::Lambda::body));
            locals.push_back({
              .value = expression::tree::Arg{depth},
              .type = lambda_domain
            });
            auto body = evaluate(lambda.body, depth + 1);
            locals.pop_back();
            position.steps.back() = resolution::output::path_segment_of(&resolution::output::Lambda::type);
            auto type = evaluate(lambda.type, depth);
            position.steps.pop_back();

            auto lambda_declaration = make_variable(depth, variable_explanation::LambdaDeclaration{position});
            auto lambda_codomain = make_variable(depth, variable_explanation::LambdaCodomain{position});
            auto lambda_body = make_variable(depth + 1, variable_explanation::LambdaCastBody{position});

            casts.push_back({
              .depth = depth + 1,
              .source_type = std::move(body.type),
              .source = std::move(body.value),
              .target_type = expression::tree::Apply{
                lambda_codomain,
                expression::tree::Arg{depth}
              },
              .target = lambda_body
            });
            rules.push_back({
              .pattern = expression::tree::Apply{
                lambda_declaration,
                expression::tree::Arg{depth}
              },
              .replacement = std::move(lambda_body)
            });
            casts.push_back({
              .depth = depth,
              .source_type = std::move(type.type),
              .source = std::move(type.value),
              .target_type = expression::tree::External{context.type_axiom},
              .target = lambda_domain
            });

            return expression::TypedValue{
              .value = std::move(lambda_declaration),
              .type = arrow_type(std::move(lambda_domain), std::move(lambda_codomain))
            };
          },
          [&](resolution::output::Local const& local) {
            return locals.at(local.stack_index);
          },
          [&](resolution::output::Hole const&) {
            return expression::TypedValue{
              .value = make_variable(depth, variable_explanation::ExplicitHoleValue{position}),
              .type = make_variable(depth, variable_explanation::ExplicitHoleType{position})
            };
          },
          [&](resolution::output::Embed const& embed) {
            return context.embed(embed.embed_index);
          }
        });
      }
    };
    Detail detail{
      .context = context,
      .get_function_parts = std::move(get_function_parts_out)
    };
    auto result = detail.evaluate(tree, 0);
    return {
      .variables = std::move(detail.variables),
      .casts = std::move(detail.casts),
      .rules = std::move(detail.rules),
      .result = std::move(result)
    };
  }
}
