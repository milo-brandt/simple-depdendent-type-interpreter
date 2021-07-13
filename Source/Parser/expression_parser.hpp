#ifndef EXPRESSION_PARSER_HPP
#define EXPRESSION_PARSER_HPP

#include "parser.hpp"
#include "recursive_macros.hpp"
#include "interface.hpp"

/*

namespace parser {
  namespace standard {
    struct Result {
      tree::Node value;
      tree_locator::Node locator;
    };

    constexpr auto identifier = map(symbol("id"), [](Empty) -> std::string_view { return {}; });

    MB_DECLARE_RECURSIVE_PARSER(term, Result);
    MB_DECLARE_RECURSIVE_PARSER(expression, Result);

    constexpr auto apply_collector = [](Result& lhs) { //we can safely move the reference when we return a success
      return map(sequence(maybe_whitespace, term), [&lhs](Result rhs) -> Result {
        return {
          .value = tree::Apply{std::move(lhs.value), std::move(rhs.value)},
          .locator = tree_locator::Apply{"hi", std::move(lhs.locator), std::move(rhs.locator)}
        };
      });
    };
    constexpr auto basic_expression = bind_fold(term, apply_collector);
    constexpr auto lambda_expression = map(loose_sequence(
      loose_symbol("\\"),
      optional(identifier),
      optional(sequence(loose_symbol(":"), expression)),
      loose_symbol("."),
      expression
    ), unpacked([](std::optional<std::string_view> arg_name, std::optional<Result> type_declaration, Result body) -> Result {
      return {
        .value = tree::Lambda {
          .arg_name = std::move(arg_name),
          .type_declaration = [&]() -> std::optional<tree::Node> {
            if(type_declaration) {
              return std::move(type_declaration->value);
            } else {
              return {};
            }
          }(),
          .body = std::move(body.value)
        },
        .locator = tree_locator::Lambda {
          .position = "hi",
          .type_declaration = [&]() -> std::optional<tree_locator::Node> {
            if(type_declaration) {
              return std::move(type_declaration->locator);
            } else {
              return {};
            }
          }(),
          .body = std::move(body.locator)
        }
      };
    }));

    constexpr auto expression_with_arrows = bind(basic_expression, [](Result domain) {
      return condition_branch(loose_symbol("->"),
        map(sequence(loose_symbol("->"), expression), [&](Result codomain) -> tree::Node {
          return tree::Arrow{
            .domain = std::move(domain),
            .codomain = std::move(codomain)
          };
        }),
        map(always_match, [&](Empty) { return std::move(domain); })
      );
    });
    constexpr auto dependent_arrow_expression = map(loose_sequence(
      loose_symbol("("),
      identifier,
      symbol(":"),
      expression,
      symbol(")"),
      symbol("->"),
      expression
    ), unpacked([](std::string_view identifier, Result domain, Result codomain) -> tree::Node {
      return tree::Arrow{
        .arg_name = identifier,
        .domain = std::move(domain),
        .codomain = std::move(codomain)
      };
    }));

    MB_DEFINE_RECURSIVE_PARSER(term, branch(
      std::make_pair(loose_symbol("("), loose_sequence(loose_symbol("("), expression, loose_symbol(")"))),
      std::make_pair(always_match, map(identifier, [](std::string_view identifier) -> tree::Node { return tree::Identifier{ identifier }; }))
    ));
    MB_DEFINE_RECURSIVE_PARSER(expression, branch(
      std::make_pair(sequence(loose_symbol("("), identifier, symbol(":")), dependent_arrow_expression),
      std::make_pair(loose_symbol("\\"), lambda_expression),
      std::make_pair(always_match, expression_with_arrows)
    ));

  }
}

*/

#endif
