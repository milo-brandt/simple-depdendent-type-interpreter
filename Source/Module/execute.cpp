#include "execute.hpp"
#include "../Utility/overloaded.hpp"
#include "../Utility/vector_utility.hpp"

namespace expr_module {
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  ModuleResult execute(ExecuteContext context, Core const& core, ImportInfo imports) {
    std::vector<OwnedExpression> registers;
    std::vector<OwnedExpression> exports;
    /*
      Fill up the registers with a blank axiom (which should never be read by a valid module)
    */
    registers.reserve(core.register_count);
    auto blank_register = context.arena.axiom();
    for(std::size_t i = 0; i < core.register_count; ++i) {
      registers.push_back(context.arena.copy(blank_register));
    }
    auto set_register = [&](std::uint32_t index, new_expression::OwnedExpression value) {
      context.arena.drop(std::move(registers[index]));
      registers[index] = std::move(value);
    };
    auto copy_register = [&](std::uint32_t index) {
      return context.arena.copy(registers[index]);
    };
    for(auto const& step : core.steps) {
      std::visit(mdb::overloaded{
        [&](step::Embed const& embed) {
          set_register(embed.output, context.arena.copy(imports.value_imports[embed.import_index]));
        },
        [&](step::Apply const& apply) {
          //TODO: Verify that this application is well-typed
          set_register(apply.output, context.arena.apply(
            copy_register(apply.lhs),
            copy_register(apply.rhs)
          ));
        },
        [&](step::Declare const& declare) {
          auto ret = context.arena.declaration();
          context.rule_collector.register_declaration(ret);
          set_register(declare.output, std::move(ret));
        },
        [&](step::Axiom const& axiom) {
          auto ret = context.arena.axiom();
          set_register(axiom.output, std::move(ret));
        },
        [&](step::RegisterType const& register_type) {
          context.type_collector.set_type_of(
            registers[register_type.index],
            copy_register(register_type.type)
          );
        },
        [&](step::Argument const& arg) {
          set_register(arg.output, context.arena.argument(arg.index));
        },
        [&](step::Export const& export_step) {
          exports.push_back(copy_register(export_step.value));
        },
        [&](step::Clear const& clear) {
          set_register(clear.target, context.arena.copy(blank_register));
        },
        [&](step::Rule const& rule) {
          new_expression::Rule ret = {
            .pattern = {
              .head = copy_register(rule.head),
              .body = {
                .args_captured = rule.args_captured,
                .steps = mdb::map(
                  [&](rule_step::Any const& step) {
                    return std::visit(mdb::overloaded{
                      [&](rule_step::PatternMatch const& match) -> new_expression::PatternStep {
                        return new_expression::PatternMatch {
                          .substitution = copy_register(match.substitution),
                          .expected_head = copy_register(match.expected_head),
                          .args_captured = match.args_captured
                        };
                      },
                      [&](rule_step::DataCheck const& data_check) -> new_expression::PatternStep {
                        return new_expression::DataCheck {
                          .capture_index = data_check.capture_index,
                          .expected_type = imports.data_type_import_indices[data_check.data_type_index]
                        };
                      },
                      [&](rule_step::PullArgument const&) -> new_expression::PatternStep {
                        return new_expression::PullArgument{};
                      }
                    }, step);
                  },
                  rule.steps
                )
              }
            },
            .replacement = std::visit(mdb::overloaded{
              [&](rule_replacement::Substitution const& substitution) -> new_expression::Replacement {
                return copy_register(substitution.substitution);
              },
              [&](rule_replacement::Function const& function) -> new_expression::Replacement {
                return std::move(imports.c_replacement_imports[function.function_index]);
              }
            }, rule.replacement)
          };
          context.rule_collector.add_rule(std::move(ret));
        }
      }, step);
    }
    destroy_from_arena(context.arena, imports, blank_register, registers);
    return {
      std::move(exports)
    };
  }
}
