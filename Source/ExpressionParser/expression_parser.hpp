/*
  Syntax:
      Application:     f x
      Lambda function: \x:T.body (Argument name, type declaration optional)
      Arrow:           X -> Y
      Dependent Arrow: (x : X) -> Y

      block {
        iterate                : (T : Type) -> (T -> T) -> T -> Nat -> T;
        iterate T f x zero     = x;
        iterate T f x (succ y) = f (iterate T f x y);
        let x = y;
        iterate
      }


      block {
        axiom Nat : Type
        axiom zero : Nat
        axiom succ : Nat -> Nat
      }
*/

#ifndef EXPRESSION_PARSER_HPP
#define EXPRESSION_PARSER_HPP

#include "../Parser/parser.hpp"
#include "../Parser/recursive_macros.hpp"
#include "parser_tree.hpp"

namespace expression_parser {
  namespace parser {
    using namespace ::parser;
    MB_DECLARE_RECURSIVE_PARSER(term, located_output::Tree);
    MB_DECLARE_RECURSIVE_PARSER(expression, located_output::Tree);

    constexpr auto apply_collector = [](located_output::Tree& lhs) { //we can safely move the reference when we return a success
      return map(sequence(maybe_whitespace, capture(term)), unpacked([&lhs](located_output::Tree rhs, std::string_view span) -> located_output::Tree {
        auto lhs_position = locator::position_of(lhs.locator);
        return located_output::Apply{
          .lhs = std::move(lhs),
          .rhs = std::move(rhs),
          .position = {&*lhs_position.begin(), &*span.end()}
        };
      }));
    };

    constexpr auto basic_expression = bind_fold(term, apply_collector);
    constexpr auto lambda_expression = map(loose_capture(loose_sequence(
      symbol("\\"),
      optional(identifier),
      optional(sequence(loose_symbol(":"), expression)),
      loose_symbol("."),
      expression
    )), unpacked([](std::optional<std::string_view> arg_name, std::optional<located_output::Tree> type_declaration, located_output::Tree body, std::string_view position) -> located_output::Tree {
      if(arg_name && *arg_name == "_") arg_name = std::nullopt;
      return located_output::Lambda{
        .body = std::move(body),
        .type = std::move(type_declaration),
        .arg_name = arg_name,
        .position = position
      };
    }));

    constexpr auto expression_with_arrows = bind(basic_expression, [](located_output::Tree domain) {
      return condition_branch(loose_symbol("->"),
        map(loose_sequence(loose_symbol("->"), capture(expression)), unpacked([&](located_output::Tree codomain, std::string_view span) -> located_output::Tree {
          auto domain_span = locator::position_of(domain.locator);
          return located_output::Arrow{
            .domain = std::move(domain),
            .codomain = std::move(codomain),
            .position = {&*domain_span.begin(), &*span.end()}
          };
        })),
        map(always_match, [&](Empty) { return std::move(domain); })
      );
    });
    constexpr auto dependent_arrow_expression = map(loose_capture(loose_sequence(
      symbol("("),
      identifier,
      symbol(":"),
      expression,
      symbol(")"),
      symbol("->"),
      expression
    )), unpacked([](std::string_view identifier, located_output::Tree domain, located_output::Tree codomain, std::string_view position) -> located_output::Tree {
      std::optional<std::string_view> id = identifier;
      if(identifier == "_") id = std::nullopt;
      return located_output::Arrow{
        .domain = std::move(domain),
        .codomain = std::move(codomain),
        .arg_name = id,
        .position = position
      };
    }));
    /*
      Block stuff
    */
    /*constexpr auto declaration_prototype = map(loose_capture(loose_sequence(
      identifier,
      symbol(":"),
      expression
    )), unpacked([](std::string_view identifier, located_output::Tree type) {
      //do stuff
    }));
    constexpr auto declaration_pattern = map(loose_capture(loose_sequence(
      expression,
      symbol("="),
      expression
    )), unpacked([](located_output::Tree lhs, located_output::Tree rhs) {
      //do stuff
    }));
    constexpr auto let_expression = map(loose_capture(loose_sequence(
      symbol("let"),
      whitespace,
      identifier,
      symbol("="),
      expression
    )), unpacked([](std::string_view identifier, located_output::Tree rhs) {
      //do stuff
    }));
    constexpr auto block_action = loose_sequence(branch(
      std::make_pair(declaration_prototype, declaration_prototype),
      std::make_pair(declaration_pattern, declaration_pattern),
      std::make_pair(let_expression, let_expression)
    ), symbol(";"));

    constexpr auto block_expression = map(loose_capture(loose_sequence(
      symbol("block"),
      symbol("{"),
      zero_or_more(block_action),
      expression,
      symbol("}")
    )), unpacked([](std::string_view identifier, located_output::Tree rhs) {
      //do stuff
    }));*/

    MB_DEFINE_RECURSIVE_PARSER(term, branch(
      std::make_pair(loose_symbol("("), loose_sequence(loose_symbol("("), expression, loose_symbol(")"))),
      std::make_pair(loose_symbol("\\"), lambda_expression),
      std::make_pair(always_match, map(loosen(identifier), [](std::string_view identifier) -> located_output::Tree {
        if(identifier == "_") {
          return located_output::Hole{ .position = identifier };
        } else {
          return located_output::Identifier{ .id = identifier, .position = identifier };
        }
      }))
    ));
    MB_DEFINE_RECURSIVE_PARSER(expression, branch(
      std::make_pair(loose_sequence(loose_symbol("("), identifier, symbol(":")), dependent_arrow_expression),
      std::make_pair(always_match, expression_with_arrows)
    ));
  }
}

#endif
