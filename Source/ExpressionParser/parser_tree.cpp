#include "parser_tree.hpp"

namespace expression_parser {
  namespace {
    struct LocalContext;
    struct PatternContext;
    struct CommandContext;
    using ContextParent = std::variant<resolved::Context*, LocalContext*, CommandContext*>;
    struct LocalContext {
      std::uint64_t index;
      std::string_view name;
      ContextParent parent;
    };
    struct PatternContext {
      std::vector<LocalContext> declaration_vector;
      std::uint64_t next_index;
      ContextParent parent;
      ContextParent link_vector() {
        ContextParent p = parent;
        for(auto& local : declaration_vector) {
          local.parent = p;
          p = &local;
        }
        return p;
      }
    };
    struct CommandContext {
      std::vector<LocalContext> declaration_vector;
      std::uint64_t next_index;
      ContextParent parent;
      ContextParent link_vector() {
        ContextParent p = parent;
        for(auto& local : declaration_vector) {
          local.parent = p;
          p = &local;
        }
        return p;
      }
      void add_name(std::string_view name) {
        declaration_vector.push_back({
          .index = next_index++,
          .name = name
        });
      }
    };
    std::optional<resolved::Identifier> lookup(CommandContext& context, std::string_view str);
    std::optional<resolved::Identifier> lookup(resolved::Context& context, std::string_view str) {
      if(auto index = context.lookup(str)) {
        return resolved::Identifier{
          .is_local = false,
          .var_index = *index
        };
      } else {
        return std::nullopt;
      }
    }
    std::optional<resolved::Identifier> lookup(LocalContext& context, std::string_view str) {
      if(context.name == str) {
        return resolved::Identifier{
          .is_local = true,
          .var_index = context.index
        };
      } else {
        return std::visit([&](auto* context) { return lookup(*context, str); }, context.parent);
      }
    }
    std::optional<resolved::PatternIdentifier> lookup_pattern(PatternContext& context, std::string_view str, bool allow_wildcard) {
      for(auto& local : context.declaration_vector) {
        if(local.name == str) {
          return resolved::PatternIdentifier {
            .is_local = true,
            .var_index = local.index
          };
        }
      }
      if(auto ret = std::visit([&](auto* context) { return lookup(*context, str); }, context.parent)) {
        return resolved::PatternIdentifier {
          .is_local = ret->is_local,
          .var_index = ret->var_index
        };
      } else if(allow_wildcard) {
        auto new_index = context.next_index++;
        context.declaration_vector.push_back({
          LocalContext{ //leave .parent blank for later
            .index = new_index,
            .name = str
          }
        });
        return resolved::PatternIdentifier {
          .is_local = true,
          .var_index = new_index
        };
      } else {
        return std::nullopt;
      }
    }
    std::optional<resolved::Identifier> lookup(CommandContext& context, std::string_view str) {
      for(auto& local : context.declaration_vector) {
        if(local.name == str) {
          return resolved::Identifier {
            .is_local = true,
            .var_index = local.index
          };
        }
      }
      return std::visit([&](auto* context) { return lookup(*context, str); }, context.parent);
    }
    constexpr auto merge_errors = [](ResolutionError lhs, ResolutionError rhs) {
      for(auto& err : rhs.bad_ids) { lhs.bad_ids.push_back(std::move(err)); }
      for(auto& err : rhs.bad_pattern_ids) { lhs.bad_pattern_ids.push_back(std::move(err)); }
      return lhs;
    };
    template<class Callback, class... Ts>
    auto merged_result(Callback&& callback, Ts&&... ts) {
      return multi_error_gather_map(std::forward<Callback>(callback), merge_errors, std::forward<Ts>(ts)...);
    }
    namespace output_archive = output::archive_part;

    template<class Context, class Then>
    auto resolve_pattern(Context& context, std::uint64_t stack_depth, output_archive::Pattern const& pattern, Then&& then) {
      struct Detail {
        Context& context;
        PatternContext pattern_context;
        mdb::Result<resolved::Pattern, ResolutionError> resolve_impl(output_archive::Pattern const& pattern, bool allow_wildcard) {
          return pattern.visit(mdb::overloaded{
            [&](output_archive::PatternApply const& apply) -> mdb::Result<resolved::Pattern, ResolutionError> {
              auto lhs = resolve_impl(apply.lhs, false);
              auto rhs = resolve_impl(apply.rhs, true);
              return merged_result([&](auto lhs, auto rhs) -> resolved::Pattern {
                return resolved::PatternApply{
                  .lhs = std::move(lhs),
                  .rhs = std::move(rhs)
                };
              }, std::move(lhs), std::move(rhs));
            },
            [&](output_archive::PatternIdentifier const& id) -> mdb::Result<resolved::Pattern, ResolutionError> {
              if(auto ret = lookup_pattern(pattern_context, id.id, allow_wildcard)) {
                return resolved::Pattern{std::move(*ret)};
              } else {
                return ResolutionError{.bad_pattern_ids = {id.index()}};
              }
            },
            [&](output_archive::PatternHole const& hole) -> mdb::Result<resolved::Pattern, ResolutionError> {
              return resolved::Pattern{resolved::PatternHole {
                .var_index = pattern_context.next_index++
              }};
            }
          });
        }
      };
      Detail detail{
        .context = context,
        .pattern_context = {
          .next_index = stack_depth,
          .parent = &context
        }
      };
      auto ret = detail.resolve_impl(pattern, false);
      return std::visit([&](auto* context) {
        return then(*context, detail.pattern_context.next_index, std::move(ret));
      }, detail.pattern_context.link_vector());
    }
    mdb::Result<resolved::Command, ResolutionError> resolve_impl(CommandContext& command_context, output_archive::Command const& command);
    template<class Context>
    mdb::Result<resolved::Expression, ResolutionError> resolve_impl(Context& context, std::uint64_t stack_depth, output_archive::Expression const& expression) {
      return expression.visit(mdb::overloaded{
        [&](output_archive::Apply const& apply) -> mdb::Result<resolved::Expression, ResolutionError> {
          return merged_result([&](resolved::Expression lhs, resolved::Expression rhs) -> resolved::Expression {
            return resolved::Apply{
              .lhs = std::move(lhs),
              .rhs = std::move(rhs)
            };
          }, resolve_impl(context, stack_depth, apply.lhs), resolve_impl(context, stack_depth, apply.rhs));
        },
        [&](output_archive::Lambda const& lambda) -> mdb::Result<resolved::Expression, ResolutionError> {
          auto body_result = [&] {
            if(lambda.arg_name) {
              LocalContext local_context {
                .index = stack_depth,
                .name = *lambda.arg_name,
                .parent = &context
              };
              return resolve_impl(local_context, stack_depth + 1, lambda.body);
            } else {
              return resolve_impl(context, stack_depth + 1, lambda.body);
            }
          } ();
          if(lambda.type) {
            auto type_result = resolve_impl(context, stack_depth, *lambda.type);
            return merged_result([&](resolved::Expression body, resolved::Expression type) -> resolved::Expression {
              return resolved::Lambda{
                .body = std::move(body),
                .type = std::move(type)
              };
            }, std::move(body_result), std::move(type_result));
          } else {
            if(body_result.holds_success()) {
              return resolved::Expression{resolved::Lambda{
                .body = std::move(body_result.get_value()),
                .type = std::nullopt
              }};
            } else {
              return std::move(body_result);
            }
          }
        },
        [&](output_archive::Identifier const& identifier) -> mdb::Result<resolved::Expression, ResolutionError> {
          if(auto output = lookup(context, identifier.id)) {
            return resolved::Expression{*output};
          } else {
            return ResolutionError{.bad_ids = {identifier.index()}};
          }
        },
        [&](output_archive::Hole const& hole) -> mdb::Result<resolved::Expression, ResolutionError> {
          return resolved::Expression{resolved::Hole{}};
        },
        [&](output_archive::Arrow const& arrow) -> mdb::Result<resolved::Expression, ResolutionError> {
          auto domain_result = resolve_impl(context, stack_depth, arrow.domain);
          auto codomain_result = [&] {
            if(arrow.arg_name) {
              LocalContext local_context {
                .index = stack_depth,
                .name = *arrow.arg_name,
                .parent = &context
              };
              return resolve_impl(local_context, stack_depth + 1, arrow.codomain);
            } else {
              return resolve_impl(context, stack_depth + 1, arrow.codomain);
            }
          } ();
          return merged_result([&](resolved::Expression domain, resolved::Expression codomain) -> resolved::Expression {
            return resolved::Arrow{
              std::move(domain),
              std::move(codomain)
            };
          }, std::move(domain_result), std::move(codomain_result));
        },
        [&](output_archive::Block const& block) -> mdb::Result<resolved::Expression, ResolutionError> {
          CommandContext inner_context {
            .next_index = stack_depth,
            .parent = &context
          };
          std::vector<ResolutionError> errors;
          std::vector<resolved::Command> commands;
          for(auto const& statement : block.statements) {
            auto ret = resolve_impl(inner_context, statement);
            if(ret.holds_success()) {
              if(errors.empty()) {
                commands.push_back(std::move(ret.get_value()));
              }
            } else {
              errors.push_back(std::move(ret.get_error()));
              commands.clear();
            }
          }
          auto expr_ret = resolve_impl(inner_context, inner_context.next_index, block.value);
          if(expr_ret.holds_error()) {
            errors.push_back(std::move(expr_ret.get_error()));
          }
          if(errors.empty()) {
            return resolved::Expression{resolved::Block{
              .statements = std::move(commands),
              .value = std::move(expr_ret.get_value())
            }};
          } else {
            ResolutionError err = std::move(errors[0]);
            for(std::uint64_t i = 1; i < errors.size(); ++i) {
              err = merge_errors(std::move(err), std::move(errors[i]));
            }
            return std::move(err);
          }
        }
      });
    }
    mdb::Result<resolved::Command, ResolutionError> resolve_impl(CommandContext& command_context, output_archive::Command const& command) {
      return command.visit(mdb::overloaded{
        [&](output_archive::Declare const& declaration) -> mdb::Result<resolved::Command, ResolutionError> {
          auto ret = map(resolve_impl(command_context, command_context.next_index, declaration.type), [&](resolved::Expression expr) -> resolved::Command {
            return resolved::Declare{
              .type = std::move(expr)
            };
          });
          command_context.add_name(declaration.name);
          return ret;
        },
        [&](output_archive::Rule const& rule) -> mdb::Result<resolved::Command, ResolutionError> {
          return resolve_pattern(command_context, command_context.next_index, rule.pattern, [&](auto& inner_context, std::uint64_t inner_stack_depth, mdb::Result<resolved::Pattern, ResolutionError> pattern) {
            return merged_result([&](resolved::Pattern pattern, resolved::Expression replacement) -> resolved::Command {
              return resolved::Rule{
                .pattern = std::move(pattern),
                .replacement = std::move(replacement),
                .args_in_pattern = inner_stack_depth - command_context.next_index
              };
            }, std::move(pattern), resolve_impl(inner_context, inner_stack_depth, rule.replacement));
          });
        },
        [&](output_archive::Axiom const& axiom) -> mdb::Result<resolved::Command, ResolutionError> {
          auto ret = map(resolve_impl(command_context, command_context.next_index, axiom.type), [&](resolved::Expression expr) -> resolved::Command {
            return resolved::Axiom{
              .type = std::move(expr)
            };
          });
          command_context.add_name(axiom.name);
          return ret;
        },
        [&](output_archive::Let const& let) -> mdb::Result<resolved::Command, ResolutionError> {
          auto ret = [&] () -> mdb::Result<resolved::Command, ResolutionError> {
            auto value_result = resolve_impl(command_context, command_context.next_index, let.value);
            if(let.type) {
              auto type_result = resolve_impl(command_context, command_context.next_index, *let.type);
              return merged_result([&](resolved::Expression body, resolved::Expression type) -> resolved::Command {
                return resolved::Let{
                  .value = std::move(body),
                  .type = std::move(type)
                };
              }, std::move(value_result), std::move(type_result));
            } else {
              if(value_result.holds_success()) {
                return resolved::Command{resolved::Let{
                  .value = std::move(value_result.get_value()),
                  .type = std::nullopt
                }};
              } else {
                return std::move(value_result.get_error());
              }
            }
          } ();
          command_context.add_name(let.name);
          return ret;
        }
      });
    }
  }
  mdb::Result<resolved::Expression, ResolutionError> resolve(resolved::Context context, output::archive_part::Expression const& expression) {
    return resolve_impl(context, 0, expression);
  }
  mdb::Result<resolved::Command, ResolutionError> resolve(resolved::Context context, output::archive_part::Command const& command) {
    CommandContext command_context {
      .next_index = 0,
      .parent = &context
    };
    return resolve_impl(command_context, command);
  }
}
