#include "evaluation_context_interpreter.hpp"
#include <unordered_map>

namespace expression {
  using namespace compiler::flat;
  namespace {
    Rule make_rule(tree::Tree const& tree, tree::Tree const& replacement) {
      struct Impl {
        std::uint64_t arg_index = 0;
        std::unordered_map<std::uint64_t, std::uint64_t> arg_map;
        pattern::Tree convert(tree::Tree const& tree) {
          return tree.visit(mdb::overloaded{
            [&](tree::Apply const& apply) -> pattern::Tree {
              return pattern::Apply{
                .lhs = convert(apply.lhs),
                .rhs = convert(apply.rhs)
              };
            },
            [&](tree::External const& external) -> pattern::Tree {
              return pattern::Fixed{external.index};
            },
            [&](tree::Arg const& arg) -> pattern::Tree {
              if(arg_map.contains(arg.index)) {
                throw std::runtime_error("Arg used multiple times in pattern");
              } else {
                arg_map.insert(std::make_pair(arg.index, arg_index++));
              }
              return pattern::Wildcard{};
            }
          });
        }
        tree::Tree convert_replacement(tree::Tree const& replacement) {
          return replacement.visit(mdb::overloaded{
            [&](tree::Apply const& apply) -> tree::Tree {
              return tree::Apply{
                .lhs = convert_replacement(apply.lhs),
                .rhs = convert_replacement(apply.rhs)
              };
            },
            [&](tree::External const& external) -> tree::Tree {
              return external;
            },
            [&](tree::Arg const& arg) -> tree::Tree {
              if(!arg_map.contains(arg.index)) {
                throw std::runtime_error("Unbound arg used in replacement");
              }
              return tree::Arg{arg_map.at(arg.index)};
            }
          });
        }
      };
      Impl i;
      auto pattern = i.convert(tree);
      auto repl = i.convert_replacement(replacement);
      return Rule{
        .pattern = std::move(pattern),
        .replacement = std::move(repl)
      };
    }
    tree::Tree apply_args(tree::Tree inner, std::uint64_t arg_ct) {
      for(std::size_t i = 0; i < arg_ct; ++i) {
        inner = tree::Apply{
          .lhs = std::move(inner),
          .rhs = tree::Arg{i}
        };
      }
      return inner;
    }
  }
  InterpretResult interpret_program(instruction::Program const& program, Context& context, EmbedInfo const& embed) {
    std::vector<std::optional<TypedValue> > values;
    std::vector<std::uint64_t> context_depths;
    std::vector<solve::ConstraintSpecification> constraints;
    std::vector<solve::CastSpecification> casts;
    std::vector<std::uint64_t> holes;
    auto get_function_parts = [function_pattern = tree::match::Apply{
      tree::match::Apply{
        tree::full_match::External{tree::match::Predicate{[arrow_axiom = context.primitives.arrow](std::uint64_t index){ return index == arrow_axiom; }}},
        tree::match::Any{}
      },
      tree::match::Any{}
    }](tree::Tree const& tree) -> std::optional<std::pair<tree::Tree, tree::Tree> > {
      if(auto match = function_pattern.try_match(tree)) {
        return std::make_pair(match->lhs.rhs, match->rhs);
      } else {
        return std::nullopt;
      }
    };
    for(auto const& instruction : program.instructions) {
      auto [value, depth] = std::visit(mdb::overloaded{
        [&](instruction::Foundation const& foundation) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          if(foundation == instruction::Foundation::type) {
            return {TypedValue{
              .value = tree::External{context.primitives.type},
              .type = tree::External{context.primitives.type}
            }, 0};
          } else if(foundation == instruction::Foundation::arrow) {
            return {TypedValue{
              .value = tree::External{context.primitives.arrow},
              .type = context.primitives.arrow_type()
            }, 0};
          } else {
            return {std::nullopt, 0};
          }
        },
        [&](instruction::Hole const& hole) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto ret = apply_args(tree::External{context.external_info.size()}, context_depths[hole.context]); //need to apply args!
          holes.push_back(context.external_info.size());
          context.external_info.push_back({ .is_axiom = false });
          return {TypedValue{
            .value = std::move(ret),
            .type = values[hole.type]->value
          }, context_depths[hole.context]};
        },
        [&](instruction::Declaration const& declaration) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto ret = apply_args(tree::External{context.external_info.size()}, context_depths[declaration.context]); //need to apply args!
          context.external_info.push_back({ .is_axiom = false });
          return {TypedValue{
            .value = std::move(ret),
            .type = values[declaration.type]->value
          }, context_depths[declaration.context]};
        },
        [&](instruction::Context const& context) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto depth = context_depths[context.base_context];
          return {TypedValue{
            .value = tree::Arg{depth},
            .type = values[context.next_type]->value
          }, depth + 1};
        },
        [&](instruction::Embed const& e) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          return {embed.values.at(e.embed_index), 0};
        },
        [&](instruction::Apply const& apply) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto const& lhs = values[apply.lhs];
          auto const& rhs = values[apply.rhs];
          auto depth = std::max(context_depths[apply.lhs], context_depths[apply.rhs]);
          auto [lhs_value, lhs_domain, lhs_codomain] = [&]() -> std::tuple<tree::Tree, tree::Tree, tree::Tree> {
            if(auto lhs_func_type = get_function_parts(context.reduce(lhs->type))) {
              return std::make_tuple(lhs->value, std::move(lhs_func_type->first), std::move(lhs_func_type->second));
            } else {
              auto dom_var = context.add_declaration();
              auto codom_var = context.add_declaration();
              auto cast_var = context.add_declaration();

              auto dom_app = apply_args(tree::External{dom_var}, depth);
              auto codom_app = apply_args(tree::External{codom_var}, depth);
              auto cast_app = apply_args(tree::External{cast_var}, depth);

              holes.push_back(dom_var);
              holes.push_back(codom_var);
              holes.push_back(cast_var);

              casts.push_back({
                .depth = depth,
                .source_type = lhs->type,
                .source = lhs->value,
                .target_type = tree::Apply{
                  tree::Apply{
                    tree::External{context.primitives.arrow},
                    tree::External{dom_var}
                  },
                  tree::External{codom_var}
                },
                .cast_result = cast_var
              });
              return std::make_tuple(cast_app, dom_app, codom_app);
            }
          }();
          if(rhs->type == lhs_domain) {
            return {TypedValue{
              .value = tree::Apply{lhs_value, rhs->value},
              .type = tree::Apply{lhs_codomain, rhs->value}
            }, depth};
          } else {
            auto cast_var = context.add_declaration();
            holes.push_back(cast_var);
            auto cast_app = apply_args(tree::External{cast_var}, depth);
            casts.push_back({
              .depth = depth,
              .source_type = rhs->type,
              .source = rhs->value,
              .target_type = lhs_domain,
              .cast_result = cast_var
            });
            return {TypedValue{
              .value = tree::Apply{lhs_value, cast_app},
              .type = tree::Apply{lhs_codomain, cast_app}
            }, depth};
          }
        },
        [&](instruction::Rule const& rule) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          if(context_depths[rule.replacement] > context_depths[rule.pattern]) {
            throw std::runtime_error("Replacement context must not exceed pattern context");
          }
          auto depth = context_depths[rule.pattern];
          auto cast_var = context.add_declaration();
          holes.push_back(cast_var);
          auto cast_app = apply_args(tree::External{cast_var}, depth);
          casts.push_back({
            .depth = depth,
            .source_type = values[rule.replacement]->type,
            .source = values[rule.replacement]->value,
            .target_type = values[rule.pattern]->type,
            .cast_result = cast_var
          });

          context.rules.push_back(make_rule(values[rule.pattern]->value, cast_app));
          return {std::nullopt, depth};
        },
        [&](instruction::Cast const& cast) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          return {values[cast.from_value], context_depths[cast.from_value]};
        },
        [&](instruction::TypeFamilyOver const& type_family) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          return {TypedValue{
            .value = tree::Apply{
              .lhs = tree::Apply{
                .lhs = tree::External{context.primitives.arrow},
                .rhs = values[type_family.domain]->value
              },
              .rhs = tree::External{context.primitives.type_constant_function}
            },
            .type = tree::External{context.primitives.type}
          }, context_depths[type_family.domain]};
        }
      }, instruction);
      values.push_back(std::move(value));
      context_depths.push_back(depth);
    }
    return {
      .constraints = std::move(constraints),
      .casts = std::move(casts),
      .holes = std::move(holes),
      .result = *values[program.ret_index]
    };
  }
}
