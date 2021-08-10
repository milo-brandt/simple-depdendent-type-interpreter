//#include "WebInterface/web_interface.hpp"
#include "ExpressionParser/parser_tree.hpp"

#include "ExpressionParser/expression_parser.hpp"

#include <iostream>
#include "Utility/overloaded.hpp"
#include "Compiler/resolved_tree.hpp"
#include "Compiler/evaluator.hpp"
#include "Expression/solver.hpp"
#include "Expression/solver_context.hpp"
#include "Expression/formatter.hpp"

#include <sstream>

std::optional<std::pair<expression::pattern::Tree, std::unordered_map<std::uint64_t, std::uint64_t> > > as_pattern(expression::tree::Tree const& tree, expression::Context& context) {
  struct Detail {
    expression::Context& context;
    std::unordered_map<std::uint64_t, std::uint64_t> map;
    std::optional<expression::pattern::Tree> evaluate(expression::tree::Tree const& tree, bool on_spine) {
      return tree.visit(mdb::overloaded{
        [&](expression::tree::Apply const& apply) -> std::optional<expression::pattern::Tree> {
          auto lhs = evaluate(apply.lhs, on_spine);
          if(!lhs) return std::nullopt;
          auto rhs = evaluate(apply.rhs, false);
          if(!rhs) return std::nullopt;
          return expression::pattern::Apply{std::move(*lhs), std::move(*rhs)};
        },
        [&](expression::tree::Arg const& arg) -> std::optional<expression::pattern::Tree> {
          if(map.contains(arg.index))
            return std::nullopt;
          map.insert(std::make_pair(arg.index, map.size()));
          return expression::pattern::Wildcard{};
        },
        [&](expression::tree::External const& ext) -> std::optional<expression::pattern::Tree> {
          if(on_spine || context.external_info.at(ext.index).is_axiom) {
            return expression::pattern::Fixed{ext.index};
          } else {
            return std::nullopt;
          }
        }
      });
    }
  };
  Detail detail{context};
  auto ret = detail.evaluate(tree, true);
  if(ret) {
    return std::make_pair(std::move(*ret), std::move(detail.map));
  } else {
    return std::nullopt;
  }
}
std::optional<expression::tree::Tree> as_replacement(expression::tree::Tree const& tree, std::unordered_map<std::uint64_t, std::uint64_t> const& map) {
  struct Detail {
    std::unordered_map<std::uint64_t, std::uint64_t> const& map;
    std::optional<expression::tree::Tree> evaluate(expression::tree::Tree const& tree) {
      return tree.visit(mdb::overloaded{
        [&](expression::tree::Apply const& apply) -> std::optional<expression::tree::Tree> {
          auto lhs = evaluate(apply.lhs);
          if(!lhs) return std::nullopt;
          auto rhs = evaluate(apply.rhs);
          if(!rhs) return std::nullopt;
          return expression::tree::Apply{std::move(*lhs), std::move(*rhs)};
        },
        [&](expression::tree::Arg const& arg) -> std::optional<expression::tree::Tree> {
          if(!map.contains(arg.index))
            return std::nullopt;
          return expression::tree::Arg{map.at(arg.index)};
        },
        [&](expression::tree::External const& ext) -> std::optional<expression::tree::Tree> {
          return ext;
        }
      });
    }
  };
  Detail detail{map};
  return detail.evaluate(tree);
}
std::optional<expression::Rule> create_rule(compiler::evaluate::Rule& rule, expression::Context& context) {
  rule.pattern = context.reduce(std::move(rule.pattern));
  rule.replacement = context.reduce(std::move(rule.replacement));
  if(auto pat = as_pattern(rule.pattern, context)) {
    if(auto rep = as_replacement(rule.replacement, pat->second)) {
      return expression::Rule{
        .pattern = std::move(pat->first),
        .replacement = std::move(*rep)
      };
    }
  }
  return std::nullopt;
}
bool try_to_insert_rule(compiler::evaluate::Rule& rule, expression::Context& context) {
  if(auto r = create_rule(rule, context)) {
    context.rules.push_back(std::move(*r));
    return true;
  } else {
    return false;
  }
}

int main(int argc, char** argv) {

  expression::Context context;
  compiler::resolution::NameContext name_context;
  std::vector<expression::TypedValue> embeds;

  auto add_name = [&](std::string name, expression::TypedValue value) {
    name_context.names.insert_or_assign(std::move(name), compiler::resolution::NameInfo{
      .embed_index = embeds.size()
    });
    embeds.push_back(std::move(value));
  };

  add_name("Type", { expression::tree::External{context.primitives.type}, expression::tree::External{context.primitives.type} });
  add_name("arrow", { expression::tree::External{context.primitives.arrow}, context.primitives.arrow_type() });
  auto nat_axiom = context.add_axiom();
  add_name("Nat", { expression::tree::External{nat_axiom}, expression::tree::External{context.primitives.type} });
  add_name("zero", { expression::tree::External{context.add_axiom()}, expression::tree::External{nat_axiom}});
  auto always_nat = context.add_declaration();
  context.rules.push_back({
    .pattern = expression::pattern::Apply{
      expression::pattern::Fixed{always_nat},
      expression::pattern::Wildcard{}
    },
    .replacement = expression::tree::External{nat_axiom}
  });
  add_name("succ", { expression::tree::External{context.add_axiom()}, expression::tree::Apply{
    expression::tree::Apply{
      expression::tree::External{context.primitives.arrow},
      expression::tree::External{nat_axiom}
    },
    expression::tree::External{always_nat}
  }});

  std::size_t counter = 0;
  std::size_t rule_size = context.rules.size();
  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line == "q") return 0;

    std::string_view source = line;


    auto x = expression_parser::parser::expression(source);

    if(auto* error = parser::get_if_error(&x)) {
      std::cout << "Error: " << error->error << "\nAt char: " << (error->position.begin() - (std::string_view(source)).begin()) << "\n";
    } else {
      auto& success = parser::get_success(x);
      std::cout << "Parse result: ";
      format_indented(std::cout, success.value.locator, 0, [](auto& o, auto const& v) { o << v; });
      std::cout << "\n";

      auto resolve = compiler::resolution::resolve({
        .tree = success.value.output,
        .context = name_context,
        .embed_arrow = context.primitives.arrow
      });

      if(auto* err = resolve.get_if_error()) {
        std::cout << "Error!\n";
      } else {
        auto& resolved = resolve.get_value();
        std::cout << "Resolve result: ";
        format_indented(std::cout, resolved.tree.output, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";

        auto evaluation_result = compiler::evaluate::evaluate_tree(
          resolved.tree.output,
          {
            .arrow_axiom = context.primitives.arrow,
            .type_axiom = context.primitives.type,
            .embed = [&](std::uint64_t index) { return embeds.at(index); },
            .allocate_variable = [&]() { return context.add_declaration(); }
          }
        );

        expression::solve::Solver solvey{expression::solve::SimpleContext{context}};
        for(auto const& var : evaluation_result.variables) {
          if(!std::holds_alternative<compiler::evaluate::variable_explanation::LambdaDeclaration>(var.second)) {
            std::cout << "Variable registered: " << var.first << "\n";
            solvey.register_variable(var.first);
          } else {
            std::cout << "Declaration registered: " << var.first << "\n";
          }
        }
        std::vector<std::tuple<std::uint64_t, std::uint64_t, expression::tree::Tree, expression::tree::Tree> > casts;
        for(auto& cast : evaluation_result.casts) {
          std::cout << "Cast registered:\n";
          std::cout << "\tDepth: " << cast.depth << "\n";
          std::cout << "\tSource Type: " << expression::format::format_expression(cast.source_type, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tSource: " << expression::format::format_expression(cast.source, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tTarget Type: " << expression::format::format_expression(cast.target_type, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tTarget: " << expression::format::format_expression(cast.target, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          auto index = solvey.add_equation({
            .depth = cast.depth,
            .lhs = std::move(cast.source_type),
            .rhs = std::move(cast.target_type)
          });
          casts.emplace_back(index, cast.depth, std::move(cast.source), std::move(cast.target));
        }
        for(auto& rule : evaluation_result.rules) {
          std::cout << "Rule registered:\n";
          std::cout << "\tPattern: " << expression::format::format_expression(rule.pattern, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tReplacement: " << expression::format::format_expression(rule.replacement, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
        }
      SOLVE_HEAD:
        bool made_progress = solvey.deduce();
        auto casts_new_end = std::remove_if(casts.begin(), casts.end(), [&](auto& cast) {
          if(solvey.is_equation_satisfied(std::get<0>(cast))) {
            solvey.add_equation({
              .depth = std::get<1>(cast),
              .lhs = std::move(std::get<2>(cast)),
              .rhs = std::move(std::get<3>(cast))
            });
            return true;
          } else {
            return false;
          }
        });
        if(casts_new_end != casts.end()) {
          made_progress = true;
          casts.erase(casts_new_end, casts.end());
        }
        auto rules_new_end = std::remove_if(evaluation_result.rules.begin(), evaluation_result.rules.end(), [&](auto& rule) {
          return try_to_insert_rule(rule, context);
        });
        if(rules_new_end != evaluation_result.rules.end()) {
          made_progress = true;
          evaluation_result.rules.erase(rules_new_end, evaluation_result.rules.end());
        }
        if(made_progress) goto SOLVE_HEAD;
        if(!evaluation_result.rules.empty()) {
          std::cout << "Rules left unsatisfied!\n";
          for(auto const& rule : evaluation_result.rules) {
            std::cout
              << "Unsatisfied: " << expression::format::format_expression(rule.pattern, context, {
                .arrow_external = context.primitives.arrow
              }).result << " := " << expression::format::format_expression(rule.replacement, context, {
              .arrow_external = context.primitives.arrow
            }).result << "\n";
          }
        }
        if(solvey.is_fully_satisfied()) {
          std::cout << "Type solving succeeded.\n";
        } else {
          std::cout << "Type solving failed.\n";
          for(auto const& part : solvey.parts()) {
            if(std::visit(mdb::overloaded{
              [&](expression::solve::equation_disposition::Open const& open) -> bool {
                std::cout << "Open equation:\n";
                return true;
              },
              [&](expression::solve::equation_disposition::AxiomMismatch const& ax) -> bool {
                std::cout << "Axiom mismatch:\n";
                return true;
              },
              [&](expression::solve::equation_disposition::ArgumentMismatch const& ax) -> bool {
                std::cout << "Argument mismatch:\n";
                return true;
              },
              [&](auto const&) { return false; }
            }, part.disposition)) {
              std::cout
                << "\tLHS: " << expression::format::format_expression(part.lhs, context, {
                  .arrow_external = context.primitives.arrow
                }).result << "\n"
                << "\tRHS: " << expression::format::format_expression(part.rhs, context, {
                .arrow_external = context.primitives.arrow
              }).result << "\n";
            }
          }
        }
        {
          auto const& open_variables = solvey.variables();
          if(!open_variables.empty()) {
            bool first = true;
            std::cout << "Open variables: ";
            for(auto var : open_variables) {
              if(first) { first = false; } else { std::cout << ", "; }
              std::cout << var;
            }
            std::cout << "\n";
          }
        }
        for(std::size_t c = rule_size; c < context.rules.size(); ++c) {
          auto const& rule = context.rules[c];
          std::cout << "Pattern: " << expression::format::format_pattern(rule.pattern, context, {
            .arrow_external = context.primitives.arrow
          }).result << " := " << expression::format::format_expression(rule.replacement, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
        }
        rule_size = context.rules.size();

        auto ret_value = context.reduce(std::move(evaluation_result.result.value));
        auto ret_type = context.reduce(std::move(evaluation_result.result.type));

        std::cout << "Pretty print:\n" << expression::format::format_expression(ret_value, context, {
          .arrow_external = context.primitives.arrow,
          .full_reduce = true
        }).result << "\n";
        std::cout << "Of type:\n" << expression::format::format_expression(ret_type, context, {
          .arrow_external = context.primitives.arrow,
          .full_reduce = true
        }).result << "\n";

        std::stringstream str;
        str << "last" << counter++;
        add_name(str.str(), {std::move(ret_value), std::move(ret_type)});
      }
    }
  }
}
