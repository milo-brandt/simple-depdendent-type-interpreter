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
  tree::Tree interpret_program(instruction::Program const& program, Context& context) {
    std::vector<std::optional<tree::Tree> > values;
    std::vector<std::uint64_t> context_depths;
    for(auto const& instruction : program.instructions) {
      auto [value, depth] = std::visit(mdb::overloaded{
        [&](instruction::Foundation const& foundation) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          if(foundation == instruction::Foundation::type) {
            return {tree::External{context.primitives.type}, -1};
          } else if(foundation == instruction::Foundation::arrow) {
            return {tree::External{context.primitives.arrow}, -1};
          } else {
            return {std::nullopt, 0};
          }
        },
        [&](instruction::Hole const& hole) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          auto ret = apply_args(tree::External{context.external_info.size()}, context_depths[hole.context]); //need to apply args!
          context.external_info.push_back({ .is_axiom = false });
          return {ret, -1};
        },
        [&](instruction::Declaration const& declaration) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          auto ret = apply_args(tree::External{context.external_info.size()}, context_depths[declaration.context]); //need to apply args!
          context.external_info.push_back({ .is_axiom = false });
          return {ret, -1};
        },
        [&](instruction::Context const& context) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          auto depth = context_depths[context.base_context];
          return {tree::Arg{depth}, depth + 1};
        },
        [&](instruction::Embed const& embed) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          return {tree::External{embed.embed_index}, -1};
        },
        [&](instruction::Apply const& apply) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          auto const& lhs = values[apply.lhs];
          auto const& rhs = values[apply.rhs];
          return {tree::Apply{*lhs, *rhs}, -1};
        },
        [&](instruction::Rule const& rule) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          context.rules.push_back(make_rule(*values[rule.pattern], *values[rule.replacement]));
          return {std::nullopt, -1};
        },
        [&](instruction::Cast const& cast) -> std::pair<std::optional<tree::Tree>, std::uint64_t> {
          return {values[cast.from_value], -1};
        }
      }, instruction);
      values.push_back(std::move(value));
      context_depths.push_back(depth);
    }
    return *values[program.ret_index];
  }
}
