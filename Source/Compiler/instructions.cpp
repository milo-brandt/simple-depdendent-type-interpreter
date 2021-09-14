#include "instructions.hpp"
#include <unordered_map>

namespace compiler::instruction {
  namespace {
    namespace resolved_archive = expression_parser::resolved::archive_part;
    struct InstructionContext {
      std::vector<std::uint64_t> locals_stack;
      std::uint64_t output_stack_size = 0;
      std::vector<located_output::Command> commands;
      located_output::Expression type() {
        return located_output::PrimitiveExpression{Primitive::type};
      }
      located_output::Expression arrow(located_output::Expression domain, located_output::Expression codomain) {
        return located_output::Apply{
          located_output::Apply{
            located_output::PrimitiveExpression{Primitive::arrow},
            std::move(domain)
          },
          std::move(codomain)
        };
      }
      located_output::Local resolved_local(std::uint64_t index) {
        return located_output::Local{
          .local_index = locals_stack[index]
        };
      }
      located_output::Local result_of(located_output::Command command) {
        commands.push_back(std::move(command));
        return located_output::Local{
          .local_index = output_stack_size++
        };
      }
      void local_result_of(located_output::Command command) {
        commands.push_back(std::move(command));
        locals_stack.push_back(output_stack_size++);
      }
      located_output::Local name_value(located_output::Expression expr) {
        return result_of(located_output::Let{
          .value = std::move(expr),
        });
      }
      template<class Callback>
      void for_all(located_output::Expression base_type, Callback&& callback) {
        std::vector<located_output::Command> old_commands;
        std::swap(commands, old_commands);
        auto old_stack_size = output_stack_size;
        locals_stack.push_back(output_stack_size);
        callback(located_output::Local{
          .local_index = output_stack_size++
        });
        locals_stack.pop_back();
        output_stack_size = old_stack_size;
        auto for_all_command = located_output::ForAll{
          .type = std::move(base_type),
          .commands = std::move(commands)
        };
        std::swap(commands, old_commands);
        commands.push_back(std::move(for_all_command));
      }
      located_output::Program as_program(located_output::Expression value) {
        return located_output::ProgramRoot {
          .commands = std::move(commands),
          .value = std::move(value)
        };
      }
      located_output::Expression compile(resolved_archive::Pattern const& pattern);
      located_output::Expression compile(resolved_archive::Expression const& expression);
      void compile(resolved_archive::Command const& command);
    };
    located_output::Expression InstructionContext::compile(resolved_archive::Pattern const& pattern) {
      return pattern.visit(mdb::overloaded{
        [&](resolved_archive::PatternApply const& apply) -> located_output::Expression {
          return located_output::Apply{
            .lhs = compile(apply.lhs),
            .rhs = compile(apply.rhs)
          };
        },
        [&](resolved_archive::PatternIdentifier const& id) -> located_output::Expression {
          if(id.is_local) {
            return resolved_local(id.var_index);
          } else {
            return located_output::Embed{ .embed_index = id.var_index };
          }
        },
        [&](resolved_archive::PatternHole const& hole) -> located_output::Expression {
          return resolved_local(hole.var_index);
        }
      });
    }
    located_output::Expression InstructionContext::compile(resolved_archive::Expression const& expression) {
      return expression.visit(mdb::overloaded{
        [&](resolved_archive::Apply const& apply) -> located_output::Expression {
          return located_output::Apply {
            .lhs = compile(apply.lhs),
            .rhs = compile(apply.rhs)
          };
        },
        [&](resolved_archive::Lambda const& lambda) -> located_output::Expression {
          auto domain = [&] {
            if(lambda.type) {
              return name_value(compile(*lambda.type));
            } else {
              return result_of(located_output::DeclareHole{
                .type = type()
              });
            }
          } ();
          auto codomain = result_of(located_output::DeclareHole{
            .type = located_output::TypeFamilyOver{domain}
          });
          auto lambda_type = arrow(domain, codomain);
          auto ret = result_of(located_output::Declare{
            .type = std::move(lambda_type)
          });
          for_all(domain, [&](located_output::Local arg) {
            commands.push_back(located_output::Rule{
              .pattern = located_output::Apply{
                .lhs = ret,
                .rhs = arg
              },
              .replacement = compile(lambda.body)
            });
          });
          return ret;
        },
        [&](resolved_archive::Identifier const& id) -> located_output::Expression {
          if(id.is_local) {
            return resolved_local(id.var_index);
          } else {
            return located_output::Embed{ .embed_index = id.var_index };
          }
        },
        [&](resolved_archive::Hole const& hole) -> located_output::Expression {
          return result_of(located_output::DeclareHole{
            .type = result_of(located_output::DeclareHole{
              .type = type()
            })
          });
        },
        [&](resolved_archive::Arrow const& arrow) -> located_output::Expression {
          auto domain = name_value(compile(arrow.domain));
          auto codomain = result_of(located_output::Declare{
            .type = located_output::TypeFamilyOver{domain}
          });
          for_all(domain, [&](located_output::Local arg) {
            commands.push_back(located_output::Rule{
              .pattern = located_output::Apply{
                .lhs = codomain,
                .rhs = arg
              },
              .replacement = compile(arrow.codomain)
            });
          });
          return this->arrow(domain, codomain);
        },
        [&](resolved_archive::Block const& block) -> located_output::Expression {
          for(auto const& command : block.statements) {
            compile(command);
          }
          return compile(block.value);
        }
      });
    }
    void InstructionContext::compile(resolved_archive::Command const& command) {
      return command.visit(mdb::overloaded{
        [&](resolved_archive::Declare const& declare) {
          local_result_of(located_output::Declare{
            .type = compile(declare.type)
          });
        },
        [&](resolved_archive::Rule const& rule) {
          struct ForAllCallback {
            InstructionContext& me;
            resolved_archive::Rule const& rule;
            std::uint64_t args_left;
            void act() {
              if(args_left-- > 0) {
                me.for_all(me.result_of(located_output::DeclareHole{
                  .type = me.type()
                }), *this);
              } else {
                me.commands.push_back(located_output::Rule{
                  .pattern = me.compile(rule.pattern),
                  .replacement = me.compile(rule.replacement)
                });
              }
            }
            void operator()(located_output::Local&&) { act(); }
          };
          ForAllCallback{*this, rule, rule.args_in_pattern}.act();
        },
        [&](resolved_archive::Axiom const& axiom) {
          local_result_of(located_output::Axiom{
            .type = compile(axiom.type)
          });
        },
        [&](resolved_archive::Let const& let) {
          local_result_of(located_output::Let{
            .value = compile(let.value),
            .type = [&]() -> std::optional<located_output::Expression> {
              if(let.type) {
                return compile(*let.type);
              } else {
                return std::nullopt;
              }
            } ()
          });
        }
      });
    }
  }
  located_output::Program make_instructions(expression_parser::resolved::archive_part::Expression const& expression) {
    InstructionContext detail;
    return detail.as_program(detail.compile(expression));
  }
}
