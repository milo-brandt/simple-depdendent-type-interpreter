#include "formatter.hpp"
#include <sstream>

namespace expression::format {
  std::ostream& operator<<(std::ostream& o, Formatter format) {
    struct Options {
      std::ostream& o;
      bool parenthesize_application;
      bool parenthesize_lambda;
      bool parenthesize_arrow;
      std::uint64_t arg_count;
    };
    struct Response {
      std::unordered_set<std::uint64_t> args_used;
      void merge(Response other) {
        for(auto arg : other.args_used) args_used.insert(arg);
      }
    };
    struct Detail {
      FormatContext& context;
      tree::Expression reduce_legal(tree::Expression expr) {
        return context.expression_context.reduce_filer_rules(std::move(expr), [&](Rule const& rule) {
          return context.force_expansion(get_pattern_head(rule.pattern));
        });
      }
      bool treat_as_lambda(tree::Expression const& expr) {
        for(auto const& rule : context.expression_context.rules) {
          if(context.force_expansion(get_pattern_head(rule.pattern))) {
            pattern::Pattern const* segment = &rule.pattern;
            while(auto* apply = segment->get_if_apply()) {
              if(!apply->rhs.holds_wildcard()) break;
              segment = &apply->lhs;
              if(term_matches(expr, *segment)) {
                return true;
              }
            }
          }
        }
        return false;
      }
      Response write_expression(tree::Expression expr, Options options) {
        expr = reduce_legal(std::move(expr));
        if(auto* lhs_apply = expr.get_if_apply()) { //is it an arrow expression?
          if(auto* inner_apply = lhs_apply->lhs.get_if_apply()) {
            if(auto* lhs_ext = inner_apply->lhs.get_if_external()) {
              if(lhs_ext->external_index == context.expression_context.primitives.arrow) {
                auto& domain = inner_apply->rhs;
                auto& codomain = lhs_apply->rhs;
                std::stringstream temp;
                auto response = write_expression(tree::Apply{
                  std::move(codomain),
                  tree::Arg{options.arg_count}
                }, {
                  .o = temp,
                  .parenthesize_application = false,
                  .parenthesize_lambda = false,
                  .parenthesize_arrow = false,
                  .arg_count = options.arg_count + 1
                });
                if(options.parenthesize_arrow) options.o << "(";
                if(response.args_used.contains(options.arg_count)) {
                  options.o << "($" << options.arg_count << " : ";
                }
                response.merge(write_expression(std::move(domain), {
                  .o = options.o,
                  .parenthesize_application = false,
                  .parenthesize_lambda = true,
                  .parenthesize_arrow = true,
                  .arg_count = options.arg_count
                }));
                if(response.args_used.contains(options.arg_count)) {
                  options.o << ")";
                }
                options.o << " -> " << temp.str();
                if(options.parenthesize_arrow) options.o << ")";
                return response;
              }
            }
          }
        }
        if(treat_as_lambda(expr)) {
          if(options.parenthesize_lambda) options.o << "(";
          options.o << "\\$" << options.arg_count << ".";
          auto response = write_expression(tree::Apply{
            std::move(expr),
            tree::Arg{options.arg_count}
          }, {
            .o = options.o,
            .parenthesize_application = false,
            .parenthesize_lambda = false,
            .parenthesize_arrow = false,
            .arg_count = options.arg_count + 1
          });
          if(options.parenthesize_lambda) options.o << ")";
          return response;
        }
        return expr.visit(mdb::overloaded{
          [&](tree::Apply& apply) {
            if(options.parenthesize_application) options.o << "(";
            auto response = write_expression(std::move(apply.lhs), {
              .o = options.o,
              .parenthesize_application = false,
              .parenthesize_lambda = true,
              .parenthesize_arrow = true,
              .arg_count = options.arg_count
            });
            options.o << " ";
            response.merge(write_expression(std::move(apply.rhs), {
              .o = options.o,
              .parenthesize_application = true,
              .parenthesize_lambda = !options.parenthesize_application,
              .parenthesize_arrow = true,
              .arg_count = options.arg_count
            }));
            if(options.parenthesize_application) options.o << ")";
            return response;
          },
          [&](tree::Arg& arg) {
            Response response{
              .args_used = {arg.arg_index}
            };
            options.o << "$" << arg.arg_index;
            return response;
          },
          [&](tree::External& external) {
            context.write_external(options.o, external.external_index);
            return Response{};
          }
        });
      }
    };
    Detail{format.context}.write_expression(format.expr, {
      .o = o,
      .parenthesize_application = false,
      .parenthesize_lambda = false,
      .parenthesize_arrow = false,
      .arg_count = 0
    });
    return o;
  }
}
