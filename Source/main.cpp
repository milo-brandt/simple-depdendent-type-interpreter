//#include "WebInterface/web_interface.hpp"
#include "ExpressionParser/parser_tree.hpp"

#include "ExpressionParser/expression_parser.hpp"

#include <iostream>
#include "Utility/overloaded.hpp"
#include "Compiler/resolved_tree.hpp"
#include "Compiler/flat_instructions.hpp"
#include "Expression/evaluation_context_interpreter.hpp"
#include "Expression/solver.hpp"
#include "Expression/solver_context.hpp"
#include "Expression/formatter.hpp"

#include <sstream>

int main(int argc, char** argv) {
  //expression::Context ctx;
/*  expression::solve::Solver solvey{expression::solve::SimpleContext{ctx}};

  auto ax = ctx.add_axiom();
  auto hole = ctx.add_declaration();
  solvey.register_variable(hole);

  expression::tree::Tree lhs = expression::tree::Apply{
    expression::tree::Apply{
      expression::tree::External{ax},
      expression::tree::Apply{
        expression::tree::External{hole},
        expression::tree::Arg{0}
      }
    },
    expression::tree::Arg{1}
  };
  expression::tree::Tree rhs = expression::tree::Apply{
    expression::tree::Apply{
      expression::tree::External{ax},
      expression::tree::Arg{0}
    },
    expression::tree::Arg{1}
  };

  solvey.add_equation({
    .depth = 2,
    .lhs = lhs,
    .rhs = rhs
  });

  std::cout << "LHS:\n";
  format_indented(std::cout, lhs, 0, [](auto& o, auto const& v) { o << v; });
  std::cout << "\nRHS:\n";
  format_indented(std::cout, rhs, 0, [](auto& o, auto const& v) { o << v; });


  solvey.deduce();

  std::cout << "\nSolved: " << solvey.is_fully_satisfied() << "\n";

  lhs = ctx.reduce(lhs);
  rhs = ctx.reduce(rhs);
  std::cout << "\nLHS:\n";
  format_indented(std::cout, lhs, 0, [](auto& o, auto const& v) { o << v; });
  std::cout << "\nRHS:\n";
  format_indented(std::cout, rhs, 0, [](auto& o, auto const& v) { o << v; });
  std::cout << "\n";*/


  expression::Context context;
  compiler::resolution::NameContext name_context;
  expression::EmbedInfo embeds;

  auto add_name = [&](std::string name, expression::TypedValue value) {
    name_context.names.insert_or_assign(std::move(name), compiler::resolution::NameInfo{
      .embed_index = embeds.values.size()
    });
    embeds.values.push_back(std::move(value));
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

        auto [program, explain] = compiler::flat::flatten_tree(resolved.tree.output);
        std::cout << "Program:\n" << program << "\n";

        auto interpret_result = expression::interpret_program(program, context, embeds);
        auto& [ret_value, ret_type] = interpret_result.result;

        expression::solve::Solver solvey{expression::solve::SimpleContext{context}};
        for(auto const& hole : interpret_result.holes) {
          std::cout << "Variable registered: " << hole << "\n";
          solvey.register_variable(hole);
        }
        for(auto& cast : interpret_result.casts) {
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
          std::cout << "\tCast Result: " << cast.cast_result << "\n";
          solvey.add_cast(std::move(cast));
        }
        for(auto& constraint : interpret_result.constraints) {
          std::cout << "Constraint registered:\n";
          std::cout << "\tDepth: " << constraint.depth << "\n";
          std::cout << "\tLHS: " << expression::format::format_expression(constraint.lhs, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";
          std::cout << "\tRHS: " << expression::format::format_expression(constraint.rhs, context, {
            .arrow_external = context.primitives.arrow
          }).result << "\n";

          solvey.add_equation(std::move(constraint));
        }

        solvey.deduce();


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
          std::cout << "Pattern: ";
          format_indented(std::cout, rule.pattern, 0, [](auto& o, auto const& v) { o << v; });
          std::cout << "\n";
          std::cout << "Replacement: ";
          format_indented(std::cout, rule.replacement, 0, [](auto& o, auto const& v) { o << v; });
          std::cout << "\n\n";
        }
        rule_size = context.rules.size();

        std::cout << "Interpreted result: ";
        format_indented(std::cout, ret_value, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";
        std::cout << "Of type: ";
        format_indented(std::cout, ret_type, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";
        ret_value = context.reduce(std::move(ret_value));
        std::cout << "Simplified: ";
        format_indented(std::cout, ret_value, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";
        ret_type = context.reduce(std::move(ret_type));
        std::cout << "Of type: ";
        format_indented(std::cout, ret_type, 0, [](auto& o, auto const& v) { o << v; });
        std::cout << "\n";

        std::cout << "Pretty print:\n" << expression::format::format_expression(ret_value, context, {
          .arrow_external = context.primitives.arrow
        }).result << "\n";
        std::cout << "Of type:\n" << expression::format::format_expression(ret_type, context, {
          .arrow_external = context.primitives.arrow
        }).result << "\n";

        std::stringstream str;
        str << "last" << counter++;
        add_name(str.str(), {std::move(ret_value), std::move(ret_type)});
      }
    }
  }
}
