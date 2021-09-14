#include "instructions.hpp"
#include <unordered_map>

namespace compiler::instruction {
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
      void for_all(located_output::Expression base_type, Explanation arg_explanation, Explanation for_all_explanation, Callback&& callback) {
        std::vector<located_output::Command> old_commands;
        std::swap(commands, old_commands);
        auto old_stack_size = output_stack_size;
        locals_stack.push_back(output_stack_size);
        callback(located_output::Local{
          .local_index = output_stack_size++,
          .source = arg_explanation
        });
        locals_stack.pop_back();
        output_stack_size = old_stack_size;
        auto for_all_command = located_output::ForAll{
          .type = std::move(base_type),
          .commands = std::move(commands),
          .source = for_all_explanation
        };
        std::swap(commands, old_commands);
        commands.push_back(std::move(for_all_command));
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
          for_all(
            domain,
            {ExplanationKind::lambda_for_all_arg, lambda.index()},
            {ExplanationKind::lambda_for_all, lambda.index()},
            [&](located_output::Local arg) {
              commands.push_back(located_output::Rule{
                .pattern = located_output::Apply{
                  .lhs = ret,
                  .rhs = arg,
                  .source = {ExplanationKind::lambda_pat_apply, lambda.index()}
                },
                .replacement = compile(lambda.body),
                .source = {ExplanationKind::lambda_rule, lambda.index()}
              });
            }
          );
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
          for_all(
            domain,
            {ExplanationKind::arrow_for_all_arg, arrow.index()},
            {ExplanationKind::arrow_for_all, arrow.index()},
            [&](located_output::Local arg) {
              commands.push_back(located_output::Rule{
                .pattern = located_output::Apply{
                  .lhs = codomain,
                  .rhs = arg,
                  .source = {ExplanationKind::arrow_pattern_apply, arrow.index()}
                },
                .replacement = compile(arrow.codomain),
                .source = {ExplanationKind::arrow_rule, arrow.index()}
              });
            }
          );
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
          struct ForAllCallback {
            InstructionContext& me;
            resolved_archive::Rule const& rule;
            std::uint64_t args_left;
            void act() {
              if(args_left-- > 0) {
                me.for_all(me.result_of(located_output::DeclareHole{
                  .type = me.type({ExplanationKind::rule_pattern_type_hole_type, rule.index()}),
                  .source = {ExplanationKind::rule_pattern_type_hole, rule.index()}
                }, {ExplanationKind::rule_pattern_type_local, rule.index()}),
                {ExplanationKind::rule_pattern_arg, rule.index()},
                {ExplanationKind::rule_pattern_for_all, rule.index()},
                *this);
              } else {
                me.commands.push_back(located_output::Rule{
                  .pattern = me.compile(rule.pattern),
                  .replacement = me.compile(rule.replacement),
                  .source = {ExplanationKind::rule, rule.index()}
                });
              }
            }
            void operator()(located_output::Local&&) { act(); }
          };
          ForAllCallback{*this, rule, rule.args_in_pattern}.act();
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
