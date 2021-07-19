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
    tree::Tree get_codomain_of(tree::Tree const& function_type, std::uint64_t arrow_axiom) {
      namespace match = tree::match;
      auto matches = match::Apply{
        match::Apply{
          tree::full_match::External{match::Predicate{[arrow_axiom](std::uint64_t index){ return index == arrow_axiom; }}},
          match::Any{}
        },
        match::Any{}
      }.try_match(function_type);
      if(!matches) {
        std::cerr << "Not a function:\n";
        format_indented(std::cerr, function_type, 0, [](auto& o, auto const& v) { o << v; });
        std::cerr << "\n";
        throw std::runtime_error("Not a function");
      } else {
        return matches->rhs;
      }
    }
  }
  TypedValue interpret_program(instruction::Program const& program, Context& context, EmbedInfo const& embed) {
    std::vector<std::optional<TypedValue> > values;
    std::vector<std::uint64_t> context_depths;
    for(auto const& instruction : program.instructions) {
      std::cout << "Parsing: " << instruction << "\n";
      auto [value, depth] = std::visit(mdb::overloaded{
        [&](instruction::Foundation const& foundation) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          if(foundation == instruction::Foundation::type) {
            return {TypedValue{
              .value = tree::External{context.primitives.type},
              .type = tree::External{context.primitives.type}
            }, -1};
          } else if(foundation == instruction::Foundation::arrow) {
            return {TypedValue{
              .value = tree::External{context.primitives.arrow},
              .type = context.primitives.arrow_type()
            }, -1};
          } else {
            return {std::nullopt, 0};
          }
        },
        [&](instruction::Hole const& hole) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto ret = apply_args(tree::External{context.external_info.size()}, context_depths[hole.context]); //need to apply args!
          context.external_info.push_back({ .is_axiom = false });
          return {TypedValue{
            .value = std::move(ret),
            .type = values[hole.type]->value
          }, -1};
        },
        [&](instruction::Declaration const& declaration) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto ret = apply_args(tree::External{context.external_info.size()}, context_depths[declaration.context]); //need to apply args!
          context.external_info.push_back({ .is_axiom = false });
          return {TypedValue{
            .value = std::move(ret),
            .type = values[declaration.type]->value
          }, -1};
        },
        [&](instruction::Context const& context) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto depth = context_depths[context.base_context];
          return {TypedValue{
            .value = tree::Arg{depth},
            .type = values[context.next_type]->value
          }, depth + 1};
        },
        [&](instruction::Embed const& e) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          return {embed.values.at(e.embed_index), -1};
        },
        [&](instruction::Apply const& apply) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          auto const& lhs = values[apply.lhs];
          auto const& rhs = values[apply.rhs];
          return {TypedValue{
            .value = tree::Apply{lhs->value, rhs->value},
            .type = tree::Apply{get_codomain_of(context.reduce(lhs->type), context.primitives.arrow), rhs->value}
          }, -1};
        },
        [&](instruction::Rule const& rule) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          context.rules.push_back(make_rule(values[rule.pattern]->value, values[rule.replacement]->value));
          return {std::nullopt, -1};
        },
        [&](instruction::Cast const& cast) -> std::pair<std::optional<TypedValue>, std::uint64_t> {
          return {values[cast.from_value], -1};
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
          }, -1};
        }
      }, instruction);
      values.push_back(std::move(value));
      context_depths.push_back(depth);
    }
    return *values[program.ret_index];
  }
}
