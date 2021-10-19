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
      std::uint64_t root_index;
      ContextParent parent;
      template<class Filter>
      std::pair<ContextParent, std::vector<std::uint64_t> > link_vector(Filter&& filter) {
        ContextParent p = parent;
        std::vector<std::size_t> indices_used;
        std::uint64_t index = 0;
        std::uint64_t local_index = root_index;
        for(auto& local : declaration_vector) {
          if(filter(local.name)) {
            indices_used.push_back(index);
            local.index = local_index++;
            local.parent = p;
            p = &local;
          }
          ++index;
        }
        return std::make_pair(p, std::move(indices_used));
      }
      void unlink_vector() {
        for(std::size_t i = 0; i < declaration_vector.size(); ++i) {
          declaration_vector[i].index = root_index + i;
        }
      }
      ContextParent link_vector() {
        return link_vector([](auto&&) { return true; }).first;
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
    resolved::Context& root_context(ContextParent& parent);
    resolved::Context& root_context(LocalContext& context) {
      return root_context(context.parent);
    }
    resolved::Context& root_context(CommandContext& context) {
      return root_context(context.parent);
    }
    resolved::Context& root_context(resolved::Context& context) {
      return context;
    }
    resolved::Context& root_context(ContextParent& parent) {
      return *std::visit([&](auto* p) -> resolved::Context* { return &root_context(*p); }, parent);
    }

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

    bool expression_uses_identifier(output_archive::Expression const& expr, std::string_view id) {
      return expr.visit(mdb::overloaded{
        [&](output_archive::Apply const& apply) {
          return expression_uses_identifier(apply.lhs, id) || expression_uses_identifier(apply.rhs, id);
        },
        [&](output_archive::Lambda const& lambda) {
          if(lambda.type && expression_uses_identifier(*lambda.type, id)) return true;
          if(lambda.arg_name && *lambda.arg_name == id) return false;
          return expression_uses_identifier(lambda.body, id);
        },
        [&](output_archive::Identifier const& identifier) {
          return identifier.id == id;
        },
        [&](output_archive::Hole const&) {
          return false;
        },
        [&](output_archive::Arrow const& arrow) {
          if(expression_uses_identifier(arrow.domain, id)) return true;
          if(arrow.arg_name && *arrow.arg_name == id) return false;
          return expression_uses_identifier(arrow.codomain, id);
        },
        [&](output_archive::VectorLiteral const& vector_literal) {
          for(auto const& element : vector_literal.elements) {
            if(expression_uses_identifier(element, id)) return true;
          }
          return false;
        },
        [&](output_archive::Literal const&) {
          return false;
        },
        [&](output_archive::Block const& block) {
          for(auto const& statement : block.statements) {
            bool shadowed = false;
            if(statement.visit(mdb::overloaded{
              [&](output_archive::Declare const& declare) {
                shadowed = declare.name == id;
                return expression_uses_identifier(declare.type, id);
              },
              [&](output_archive::Axiom const& axiom) {
                shadowed = axiom.name == id;
                return expression_uses_identifier(axiom.type, id);
              },
              [&](output_archive::Let const& let) {
                shadowed = let.name == id;
                if(let.type && expression_uses_identifier(*let.type, id)) return true;
                return expression_uses_identifier(let.value, id);
              },
              [&](output_archive::Rule const& rule) {
                for(auto const& subexpr : rule.subclause_expressions) {
                  if(expression_uses_identifier(subexpr, id)) return true;
                }
                return expression_uses_identifier(rule.replacement, id);
              }
            })) return true;
            if(shadowed) return false;
          }
          return expression_uses_identifier(block.value, id);
        }
      });
    }
    mdb::Result<resolved::Command, ResolutionError> resolve_impl(CommandContext& command_context, output_archive::Command const& command);
    template<class Context>
    mdb::Result<resolved::Expression, ResolutionError> resolve_impl(Context& context, std::uint64_t stack_depth, output_archive::Expression const& expression);
    mdb::Result<resolved::Command, ResolutionError> resolve_rule(CommandContext& context, std::uint64_t stack_depth, output_archive::Rule const& rule) {
      struct Detail {
        PatternContext pattern_context;
        mdb::Result<resolved::Pattern, ResolutionError> resolve_pattern(output_archive::Pattern const& pattern, bool allow_wildcard) {
          return pattern.visit(mdb::overloaded{
            [&](output_archive::PatternApply const& apply) -> mdb::Result<resolved::Pattern, ResolutionError> {
              auto lhs = resolve_pattern(apply.lhs, false);
              auto rhs = resolve_pattern(apply.rhs, true);
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
                return ResolutionError{
                  .bad_pattern_ids = {id.index()}
                };
              }
            },
            [&](output_archive::PatternHole const& hole) -> mdb::Result<resolved::Pattern, ResolutionError> {
              return resolved::Pattern{resolved::PatternHole {
              //  .var_index = pattern_context.next_index++
              }};
            }
          });
        }
      };
      Detail detail{
        .pattern_context = {
          .next_index = stack_depth,
          .root_index = stack_depth,
          .parent = &context
        }
      };
      auto primary_pattern = detail.resolve_pattern(rule.pattern, false);
      if(primary_pattern.holds_error()) return std::move(primary_pattern.get_error());
      std::vector<std::vector<std::uint64_t> > captures_used_in_subclause_expression;
      std::vector<resolved::Expression> subexpression_exprs;
      std::vector<ResolutionError> subexpression_expr_errors;
      std::vector<resolved::Pattern> subexpression_patterns;
      for(std::size_t i = 0; i < rule.subclause_expressions.size(); ++i) {
        auto [context, indices_used] = detail.pattern_context.link_vector([&](std::string_view id) {
          return expression_uses_identifier(rule.subclause_expressions[i], id);
        });
        captures_used_in_subclause_expression.push_back(std::move(indices_used));
        auto subexpr = std::visit([&](auto* context) {
          return resolve_impl(*context, stack_depth + indices_used.size(), rule.subclause_expressions[i]);
        }, context);
        if(subexpr.holds_error()) {
          subexpression_expr_errors.push_back(std::move(subexpr.get_error()));
        } else {
          subexpression_exprs.push_back(std::move(subexpr.get_value()));
        }
        detail.pattern_context.unlink_vector();
        auto subpattern = detail.resolve_pattern(rule.subclause_patterns[i], false);
        if(subpattern.holds_error()) {
          if(subexpression_expr_errors.empty()) {
            return std::move(subpattern.get_error());
          } else {
            auto err = std::move(subexpression_expr_errors[0]);
            for(std::size_t i = 1; i < subexpression_expr_errors.size(); ++i) {
              err = merge_errors(std::move(err), std::move(subexpression_expr_errors[i]));
            }
            return merge_errors(std::move(err), std::move(subpattern.get_error()));
          }
        } else {
          subexpression_patterns.push_back(std::move(subpattern.get_value()));
        }
      }
      auto replacement = std::visit([&](auto* context) {
        return resolve_impl(*context, stack_depth + detail.pattern_context.declaration_vector.size(), rule.replacement);
      }, detail.pattern_context.link_vector());
      if(!subexpression_expr_errors.empty()) {
        auto err = std::move(subexpression_expr_errors[0]);
        for(std::size_t i = 1; i < subexpression_expr_errors.size(); ++i) {
          err = merge_errors(std::move(err), std::move(subexpression_expr_errors[i]));
        }
        if(replacement.holds_error()) {
          err = merge_errors(std::move(err), std::move(replacement.get_error()));
        }
        return std::move(err);
      }
      if(replacement.holds_error()) {
        return std::move(replacement.get_error());
      }
      return resolved::Command{resolved::Rule{
        .pattern = std::move(primary_pattern.get_value()),
        .subclause_expressions = std::move(subexpression_exprs),
        .subclause_patterns = std::move(subexpression_patterns),
        .replacement = std::move(replacement.get_value()),
        .captures_used_in_subclause_expression = std::move(captures_used_in_subclause_expression),
        .args_in_pattern = detail.pattern_context.declaration_vector.size()
      }};
    }
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
        },
        [&](output_archive::Literal const& literal) -> mdb::Result<resolved::Expression, ResolutionError> {
          auto index = root_context(context).embed_literal(literal.value);
          return resolved::Expression{resolved::Literal{
            .embed_index = index
          }};
        },
        [&](output_archive::VectorLiteral const& vector_literal) -> mdb::Result<resolved::Expression, ResolutionError> {
          std::vector<ResolutionError> errors;
          std::vector<resolved::Expression> elements;
          for(auto const& element : vector_literal.elements) {
            auto ret = resolve_impl(context, stack_depth, element);
            if(ret.holds_success()) {
              if(errors.empty()) {
                elements.push_back(std::move(ret.get_value()));
              }
            } else {
              errors.push_back(std::move(ret.get_error()));
              elements.clear();
            }
          }
          if(errors.empty()) {
            return resolved::Expression{resolved::VectorLiteral{
              .elements = std::move(elements),
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
          return resolve_rule(command_context, command_context.next_index, rule);
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
