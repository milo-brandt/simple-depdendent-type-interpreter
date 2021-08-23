#include "instructions.hpp"
#include <unordered_map>

namespace compiler::instruction {
  namespace {
    constexpr auto merge_errors = [](Error&& lhs, Error&& rhs) {
      for(auto err : rhs.unrecognized_ids) lhs.unrecognized_ids.push_back(std::move(err));
      return std::move(lhs);
    };
    template<class Callback, class... Results>
    auto merged_gather_map(Callback&& callback, Results&&... results) {
      return multi_error_gather_map(std::forward<Callback>(callback), merge_errors, std::forward<Results>(results)...);
    }
    struct EmbedIndex { std::uint64_t index; };
    struct ExternalIndex { std::uint64_t index; };
    using LocalEntry = std::variant<EmbedIndex, ExternalIndex>;
    namespace archive = expression_parser::output::archive_part;
    namespace archive_index = expression_parser::archive_index;

    struct ResolutionData {
      Context global_context;
      std::unordered_map<std::string, LocalEntry> locals;
      std::uint64_t stack_depth = 0;
      std::vector<located_output::Command>* command_target;
      mdb::Result<located_output::Expression, Error> resolve(archive::Expression const& expression);
      mdb::Result<std::monostate, Error> resolve(archive::Command const& expression);
      located_output::Local add_stack_command(located_output::Command command) {
        command_target->push_back(std::move(command));
        return located_output::Local{.local_index = stack_depth++ };
      }
      template<class Callback>
      void for_all(located_output::Expression type, Callback&& callback) {
        std::vector<located_output::Command> new_commands;
        auto old_commands = command_target;
        command_target = &new_commands;
        auto cleanup_switch = [&] {
          command_target = old_commands;
          command_target->push_back(located_output::ForAll{
            .type = std::move(type),
            .commands = std::move(new_commands)
          });
        };
        auto run_inner = [&] () -> decltype(auto) {
          return callback(located_output::Local{.local_index = stack_depth++});
        };
        if constexpr(std::is_void_v<decltype(run_inner())>) {
          run_inner();
          cleanup_switch();
        } else {
          decltype(auto) ret = run_inner();
          cleanup_switch();
          return ret;
        }
      }
      located_output::Expression type_axiom() {
        return located_output::PrimitiveExpression{ Primitive::type };
      }
      located_output::Expression make_hole_of_type(located_output::Expression hole_type, Explanation explanation) {
        command_target->push_back(located_output::DeclareHole{
          .type = std::move(hole_type),
          .source = explanation
        });
        return located_output::Embed{
          .embed_index = stack_depth++,
          .source = explanation
        };
      }
      located_output::Expression arrow_between(located_output::Expression domain, located_output::Expression codomain) {
        return located_output::Apply{
          located_output::Apply{
            located_output::PrimitiveExpression{
              Primitive::arrow
            },
            domain
          },
          codomain
        };
      }
      located_output::Expression arrow_between_const(located_output::Expression domain, located_output::Expression codomain) {
        auto domain_local = add_stack_command(located_output::Let{
          .value = std::move(domain),
          .type = located_output::PrimitiveExpression{Primitive::type},
        });
        auto codomain_declaration = add_stack_command(located_output::Declare{
          .type = located_output::TypeFamilyOver{domain_local}
        });
        for_all(located_output::TypeFamilyOver{domain_local}, [&](auto arg) {
          command_target->push_back(located_output::Rule{
            .pattern = located_output::Apply{
              .lhs = codomain_declaration,
              .rhs = arg
            },
            .replacement = std::move(codomain)
          });
        });
        return arrow_between(std::move(domain_local), std::move(codomain_declaration));
      }
    };
    mdb::Result<located_output::Expression, Error> ResolutionData::resolve(expression_parser::output::archive_part::Expression const& expression) {
      using Ret = mdb::Result<located_output::Expression, Error>;
      return expression.visit(mdb::overloaded{
        [&](archive::Apply const& apply) -> Ret {
          return merged_gather_map([&] (located_output::Expression&& lhs, located_output::Expression&& rhs) -> located_output::Expression {
            return located_output::Apply{
              .lhs = std::move(lhs),
              .rhs = std::move(rhs),
              .source = -17
            };
          }, resolve(apply.lhs), resolve(apply.rhs));
        },
        [&](archive::Lambda const& lambda) -> Ret {
          auto body = resolve(lambda.body);
          if(body.holds_error() && !lambda.type) {
            return std::move(body); //forward the error if it's the only thing in play
          }
          auto type = [&] () -> Ret {
            if(lambda.type) {
              return resolve(*lambda.type);
            } else {
              return make_hole_of_type(type_axiom(), -17);
            }
          } ();
          return merged_gather_map([&] (located_output::Expression&& body, located_output::Expression&& type) -> located_output::Expression {
            auto domain_local = add_stack_command(located_output::Let{
              .value = std::move(type),
              .type = located_output::PrimitiveExpression{Primitive::type},
            });
            auto codomain_hole = make_hole_of_type(located_output::TypeFamilyOver{domain_local}, -17);
            auto lambda_declaration = add_stack_command(located_output::Declare{
              .type = arrow_between(domain_local, codomain_hole)
            });

          }, std::move(body), std::move(type));
        }
      });
    }
  }
  mdb::Result<located_output::Program, Error> resolve(expression_parser::output::archive_part::Expression const& expression, Context global_context) {
  }
}
