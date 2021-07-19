#include "flat_instructions.hpp"

namespace compiler::flat {
  std::pair<instruction::Program, explanation::Explanation> flatten_tree(compiler::resolution::output::Tree const& tree) {
    struct Impl {
      std::vector<instruction::Instruction> program;
      explanation::Explanation explain;
      std::uint64_t type_index;
      std::uint64_t arrow_index;
      std::uint64_t empty_context_index;
      std::vector<std::uint64_t> stack;
      std::uint64_t last_index() { return program.size() - 1; }
      std::uint64_t push(instruction::Instruction instruction) {
        program.push_back(std::move(instruction));
        return last_index();
      }
      void push_primitives() {
        type_index = push(instruction::Foundation::type);
        arrow_index = push(instruction::Foundation::arrow);
        empty_context_index = push(instruction::Foundation::empty_context);
      }
      std::uint64_t function_type(std::uint64_t domain, std::uint64_t codomain) {
        return push(instruction::Apply{
          .lhs = push(instruction::Apply{
            .lhs = arrow_index,
            .rhs = domain
          }),
          .rhs = codomain
        });
      }
      std::uint64_t type_family_over(std::uint64_t context, std::uint64_t domain) {
        auto codomain_type_type = push(instruction::Declaration{
          .context = context,
          .type = type_index
        });
        push(instruction::Rule{
          .pattern = push(instruction::Apply{
            .lhs = codomain_type_type,
            .rhs = push(instruction::Context{
              .base_context = context,
              .next_type = domain
            })
          }),
          .replacement = type_index
        });
        return codomain_type_type;
      }
      std::uint64_t flatten(compiler::resolution::output::Tree const& tree, std::uint64_t context) {
        return tree.visit(mdb::overloaded{
          [&](resolution::output::Apply const& apply) {
            std::uint64_t lhs_value = flatten(apply.lhs, context);
            std::uint64_t rhs_value = flatten(apply.rhs, context);
            auto domain = push(instruction::Hole{
              .context = context,
              .type = type_index
            });
            auto codomain = push(instruction::Hole{
              .context = context,
              .type = type_family_over(context, domain)
            });
            auto func = push(instruction::Cast{
              .from_value = lhs_value,
              .to_type = function_type(domain, codomain)
            });
            auto arg = push(instruction::Cast{
              .from_value = rhs_value,
              .to_type = domain
            });
            return push(instruction::Apply{
              .lhs = func,
              .rhs = arg
            });
          },
          [&](resolution::output::Lambda const& lambda) {
            std::uint64_t domain = push(instruction::Cast{
              .from_value = flatten(lambda.type, context),
              .to_type = type_index
            });
            std::uint64_t codomain = push(instruction::Hole{
              .context = context,
              .type = type_family_over(context, domain)
            });
            std::uint64_t new_context = push(instruction::Context{
              .base_context = context,
              .next_type = domain
            });
            stack.push_back(new_context);
            std::uint64_t body = flatten(lambda.body, new_context);
            stack.pop_back();
            std::uint64_t declaration = push(instruction::Declaration{
              .context = context,
              .type = function_type(domain, codomain)
            });
            push(instruction::Rule{
              .pattern = push(instruction::Apply{
                .lhs = declaration,
                .rhs = new_context
              }),
              .replacement = body
            });
            return declaration;
          },
          [&](resolution::output::Local const& local) {
            return stack[local.stack_index];
          },
          [&](resolution::output::Hole const&) {
            std::uint64_t type_hole = push(instruction::Hole {
              .context = context,
              .type = type_index
            });
            return push(instruction::Hole {
              .context = context,
              .type = type_hole
            });
          },
          [&](resolution::output::Embed const& embed) {
            return push(instruction::Embed{
              .context = context,
              .embed_index = embed.embed_index
            });
          }
        });
      }
    };
    Impl i{};
    i.push_primitives();
    auto ret = i.flatten(tree, i.empty_context_index);
    return {{std::move(i.program), ret}, std::move(i.explain)};
  }
}
