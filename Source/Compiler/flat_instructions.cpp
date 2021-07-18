#include "flat_instructions.hpp"

namespace compiler::flat {
  std::pair<instruction::Program, explanation::Explanation> flatten_tree(compiler::resolution::output::Tree const& tree) {
    struct Impl {
      instruction::Program program;
      explanation::Explanation explain;
      std::uint64_t last_index() { return program.size() - 1; }
      std::uint64_t push(instruction::Instruction instruction) {
        program.push_back(std::move(instruction));
        return last_index();
      }
      std::uint64_t flatten(compiler::resolution::output::Tree const& tree, std::uint64_t context) {
        tree.visit(mdb::overloaded{
          [&](resolution::output::Apply const& apply) {
            std::uint64_t lhs_value = flatten(apply.lhs, context);
            std::uint64_t rhs_value = flatten(apply.rhs, context);
            auto domain = push(instruction::DeclareVariable{
              .context = context,
              .type = (std::uint64_t)-1
            });
            auto context_with_domain = push(instruction::DeclareContext{
              .base_context = context,
              .next_type = domain
            });
            auto codomain = push(instruction::DeclareVariable{
              .context = context_with_domain,
              .type = (std::uint64_t)-1
            });
            auto func_domain_to = push(instruction::Apply{
              .lhs = (std::uint64_t)-2, // arrow
              .rhs = domain
            });
            auto func_family = push(instruction::Lambda{
              .body = codomain
            });
            auto func_type = push(instruction::Apply{
              func_domain_to,
              func_family
            });
            auto func = push(instruction::Cast{
              .from_value = lhs_value,
              .to_type = func_type
            });
            auto arg = push(instruction::Cast{
              .from_value = rhs_value,
              .to_type = domain
            });
            push(instruction::Apply{
              .lhs = func,
              .rhs = arg
            });
          },
          [&](resolution::output::Lambda const& lambda) {
            std::uint64_t type = flatten(lambda.type, context);
            std::uint64_t new_context = push(instruction::DeclareContext{
              .base_context = context,
              .next_type = type
            });
            std::uint64_t body = flatten(lambda.body, new_context);
            push(instruction::Lambda{
              .body = body
            });
          },
          [&](resolution::output::Local const& local) {
            push(instruction::Argument{
              .context = context,
              .index = local.stack_index
            });
          },
          [&](resolution::output::Hole const&) {
            std::uint64_t type_hole = push(instruction::DeclareVariable{
              .context = context,
              .type = (std::uint64_t)-1
            });
            push(instruction::DeclareVariable{
              .context = context,
              .type = type_hole
            });
          },
          [&](resolution::output::Embed const& embed) {
            push(instruction::Embed{
              .context = context,
              .embed_index = embed.embed_index
            });
          }
        });
        return last_index();
      }
    };
    Impl i{};
    i.flatten(tree, -1);
    return {std::move(i.program), std::move(i.explain)};
  }
}
