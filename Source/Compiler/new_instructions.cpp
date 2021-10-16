#include "new_instructions.hpp"
#include <unordered_map>

namespace compiler::new_instruction {
  namespace {
    namespace resolved_archive = expression_parser::resolved::archive_part;
    struct InstructionContext {
      std::vector<std::uint64_t> locals_stack;
      std::uint64_t output_stack_size = 0;
      std::vector<located_output::Command> commands;
      located_output::Expression type(Explanation explanation) {
        return located_output::PrimitiveExpression{
          Primitive::type,
          explanation
        };
      }
      located_output::Expression arrow(located_output::Expression domain, located_output::Expression codomain, Explanation arrow_primitive, Explanation inner, Explanation outer) {
        return located_output::Apply{
          located_output::Apply{
            located_output::PrimitiveExpression{
              Primitive::arrow,
              arrow_primitive
            },
            std::move(domain),
            inner
          },
          std::move(codomain),
          outer
        };
      }
      located_output::Local resolved_local(std::uint64_t index, Explanation explanation) {
        return located_output::Local{
          .local_index = locals_stack[index],
          .source = explanation
        };
      }
      located_output::Local result_of(located_output::Command command, Explanation explanation) {
        commands.push_back(std::move(command));
        return located_output::Local{
          .local_index = output_stack_size++,
          .source = explanation
        };
      }
      void local_result_of(located_output::Command command) {
        commands.push_back(std::move(command));
        locals_stack.push_back(output_stack_size++);
      }
      located_output::Local name_value(located_output::Expression expr, Explanation let_explanation, Explanation result_explanation) {
        return result_of(located_output::Let{
          .value = std::move(expr),
          .source = let_explanation
        }, result_explanation);
      }
      template<class Callback>
      auto sub_frame(std::size_t inner_arg_ct, Explanation explanation, Callback&& callback) {
        std::vector<located_output::Command> old_commands;
        std::swap(commands, old_commands);
        auto old_stack_size = output_stack_size;
        auto old_locals_size = locals_stack.size();
        for(std::size_t i = 0; i < inner_arg_ct; ++i) {
          locals_stack.push_back(output_stack_size + i);
        }
        output_stack_size += inner_arg_ct;
        auto ret = callback(old_stack_size);
        locals_stack.erase(locals_stack.begin() + old_locals_size, locals_stack.end()); //restore stack
        output_stack_size = old_stack_size;
        auto ret_commands = std::move(commands);
        std::swap(commands, old_commands);
        return std::make_pair(std::move(ret_commands), std::move(ret));
      }
      located_output::Program as_program(located_output::Expression value) {
        auto pos = value.locator.visit([](auto const& v) { return v.source.index; });
        return located_output::ProgramRoot {
          .commands = std::move(commands),
          .value = std::move(value),
          .source = {ExplanationKind::root, pos}
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
            .rhs = compile(apply.rhs),
            .source = {ExplanationKind::pattern_apply, apply.index()}
          };
        },
        [&](resolved_archive::PatternIdentifier const& id) -> located_output::Expression {
          if(id.is_local) {
            return resolved_local(id.var_index, {ExplanationKind::pattern_id, id.index()});
          } else {
            return located_output::Embed{
              .embed_index = id.var_index,
              .source = {ExplanationKind::pattern_embed, id.index()}
            };
          }
        },
        [&](resolved_archive::PatternHole const& hole) -> located_output::Expression {
          return resolved_local(hole.var_index, {ExplanationKind::pattern_hole, hole.index()});
        }
      });
    }
    located_output::Expression InstructionContext::compile(resolved_archive::Expression const& expression) {
      return expression.visit(mdb::overloaded{
        [&](resolved_archive::Apply const& apply) -> located_output::Expression {
          return located_output::Apply {
            .lhs = compile(apply.lhs),
            .rhs = compile(apply.rhs),
            .source = {ExplanationKind::direct_apply, apply.index()}
          };
        },
        [&](resolved_archive::Lambda const& lambda) -> located_output::Expression {
          auto domain = [&] {
            if(lambda.type) {
              return name_value(
                compile(*lambda.type),
                {ExplanationKind::lambda_type_local, lambda.index()},
                {ExplanationKind::lambda_type, lambda.index()}
              );
            } else {
              return result_of(located_output::DeclareHole{
                .type = type({ExplanationKind::lambda_type_hole_type, lambda.index()}),
                .source = {ExplanationKind::lambda_type_hole, lambda.index()}
              }, {ExplanationKind::lambda_type_hole_local, lambda.index()});
            }
          } ();
          auto codomain = result_of(located_output::DeclareHole{
            .type = located_output::TypeFamilyOver{
              domain,
              {ExplanationKind::lambda_codomain_hole_type, lambda.index()}
            },
            .source = {ExplanationKind::lambda_codomain_hole, lambda.index()}
          }, {ExplanationKind::lambda_codomain_hole_local, lambda.index()});
          auto lambda_type = arrow(
            domain,
            codomain,
            {ExplanationKind::lambda_type_arrow, lambda.index()},
            {ExplanationKind::lambda_type_arrow_apply, lambda.index()},
            {ExplanationKind::lambda_type, lambda.index()}
          );
          auto ret = result_of(located_output::Declare{
            .type = std::move(lambda_type),
            .source = {ExplanationKind::lambda_declaration, lambda.index()}
          }, {ExplanationKind::lambda_declaration_local, lambda.index()});
          auto [body_commands, body] = sub_frame(1, {ExplanationKind::unknown, lambda.index()}, [&](std::size_t arg_index) {
            return compile(lambda.body);
          });
          commands.push_back(located_output::Rule{
            .primary_pattern = located_output::PatternApply{
              .lhs = located_output::PatternLocal{
                .local_index = ret.local_index,
                .source = {ExplanationKind::unknown, lambda.index()}
              },
              .rhs = located_output::PatternCapture{
                .capture_index = 0,
                .source = {ExplanationKind::unknown, lambda.index()}
              },
              .source = {ExplanationKind::unknown, lambda.index()}
            },
            .commands = std::move(body_commands),
            .replacement = std::move(body),
            .source = {ExplanationKind::unknown, lambda.index()}
          });
          return ret;
        },
        [&](resolved_archive::Identifier const& id) -> located_output::Expression {
          if(id.is_local) {
            return resolved_local(id.var_index, {ExplanationKind::id_local, id.index()});
          } else {
            return located_output::Embed{
              .embed_index = id.var_index,
              .source = {ExplanationKind::id_embed, id.index()}
            };
          }
        },
        [&](resolved_archive::Hole const& hole) -> located_output::Expression {
          return result_of(located_output::DeclareHole{
            .type = result_of(located_output::DeclareHole{
              .type = type({ExplanationKind::hole_type_type, hole.index()}),
              .source = {ExplanationKind::hole_type, hole.index()}
            }, {ExplanationKind::hole_type_local, hole.index()}),
            .source = {ExplanationKind::hole, hole.index()}
          }, {ExplanationKind::hole_local, hole.index()});
        },
        [&](resolved_archive::Arrow const& arrow) -> located_output::Expression {
          auto domain = name_value(compile(arrow.domain), {ExplanationKind::arrow_domain, arrow.index()}, {ExplanationKind::arrow_domain_local, arrow.index()});
          auto codomain = result_of(located_output::Declare{
            .type = located_output::TypeFamilyOver{
              domain,
              {ExplanationKind::arrow_codomain_type_family, arrow.index()}
            },
            .source = {ExplanationKind::arrow_codomain_type, arrow.index()}
          }, {ExplanationKind::arrow_codomain, arrow.index()});
          auto [body_commands, body] = sub_frame(1, {ExplanationKind::unknown, arrow.index()}, [&](std::size_t arg_index) {
            return compile(arrow.codomain);
          });
          commands.push_back(located_output::Rule{
            .primary_pattern = located_output::PatternApply{
              .lhs = located_output::PatternLocal{
                .local_index = codomain.local_index,
                .source = {ExplanationKind::unknown, arrow.index()}
              },
              .rhs = located_output::PatternCapture{
                .capture_index = 0,
                .source = {ExplanationKind::unknown, arrow.index()}
              },
              .source = {ExplanationKind::unknown, arrow.index()}
            },
            .commands = std::move(body_commands),
            .replacement = std::move(body),
            .source = {ExplanationKind::unknown, arrow.index()}
          });
          return this->arrow(domain, codomain,
            {ExplanationKind::arrow_arrow, arrow.index()},
            {ExplanationKind::arrow_arrow_apply, arrow.index()},
            {ExplanationKind::arrow_arrow_complete, arrow.index()}
          );
        },
        [&](resolved_archive::Block const& block) -> located_output::Expression {
          for(auto const& command : block.statements) {
            compile(command);
          }
          return compile(block.value);
        },
        [&](resolved_archive::Literal const& literal) -> located_output::Expression {
          return located_output::Embed{
            .embed_index = literal.embed_index,
            .source = {ExplanationKind::literal_embed, literal.index()}
          };
        },
        [&](resolved_archive::VectorLiteral const& vector_literal) -> located_output::Expression {
          auto vector_type = result_of(located_output::DeclareHole{
            .type = type({ExplanationKind::vector_type_type, vector_literal.index()}),
            .source = {ExplanationKind::vector_type, vector_literal.index()}
          }, {ExplanationKind::vector_type_local, vector_literal.index()});
          located_output::Expression ret = located_output::Apply{
            .lhs = located_output::PrimitiveExpression{Primitive::empty_vec, {ExplanationKind::vector_empty, vector_literal.index()}},
            .rhs = vector_type,
            .source = {ExplanationKind::vector_empty_typed, vector_literal.index()}
          };
          for(auto const& element : vector_literal.elements) {
            auto element_value = result_of(located_output::Let{
              .value = compile(element),
              .type = vector_type,
              .source = {ExplanationKind::vector_element, element.index()}
            }, {ExplanationKind::vector_element_local, element.index()});
            ret = located_output::Apply{
              .lhs = located_output::Apply{
                .lhs = located_output::Apply{
                  .lhs = located_output::PrimitiveExpression{Primitive::push_vec, {ExplanationKind::vector_push, element.index()}},
                  .rhs = vector_type,
                  .source = {ExplanationKind::vector_push_typed, element.index()}
                },
                .rhs = std::move(ret),
                .source = {ExplanationKind::vector_push_vector, element.index()}
              },
              .rhs = std::move(element_value),
              .source = {ExplanationKind::vector_push_element, element.index()}
            };
          }
          return ret;
        }
      });
    }
    void InstructionContext::compile(resolved_archive::Command const& command) {
      return command.visit(mdb::overloaded{
        [&](resolved_archive::Declare const& declare) {
          local_result_of(located_output::Declare{
            .type = compile(declare.type),
            .source = {ExplanationKind::declare, declare.index()}
          });
        },
        [&](resolved_archive::Rule const& rule) {
          std::terminate();
          /*struct Detail {
            InstructionContext& me;
            std::size_t base_stack_size;
            std::size_t count_captures(resolved_archive::Pattern const& pattern) {
              return pattern.visit(mdb::overloaded{
                [&](resolved_archive::PatternApply const& apply) {
                  return 1 + count_captures(apply.lhs) + count_captures(apply.rhs);
                },
                [&](resolved_archive::PatternIdentifier const& id) -> std::size_t {
                  if(id.is_local && id.var_index >= base_stack_size) {
                    return 0; //direct capture
                  } else if(id.is_local) {
                  } else {
                    return 1; //match
                  }
                },
                [&](resolved_archive::PatternHole const&) -> std::size_t {
                  return 0;
                }
              });
            }
          };
          auto capture_count = Detail{}.count_captures(rule.pattern);
            */
        },
        [&](resolved_archive::Axiom const& axiom) {
          local_result_of(located_output::Axiom{
            .type = compile(axiom.type),
            .source = {ExplanationKind::axiom, axiom.index()}
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
            } (),
            .source = {ExplanationKind::let, let.index()}
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
