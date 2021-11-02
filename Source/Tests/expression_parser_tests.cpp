#include "../ExpressionParser/expression_generator.hpp"
#include <catch.hpp>

using namespace expression_parser;

TEST_CASE("The expression parser matches various expressions.") {
  struct Test {
    std::string_view str;
    output::Expression expected_tree;
  };
  Test test_cases[] = {
    {
      .str = "a",
      .expected_tree = output::Identifier{"a"}
    },
    {
      .str = "a b",
      .expected_tree = output::Apply{
        output::Identifier{"a"},
        output::Identifier{"b"}
      }
    },
    {
      .str = "_",
      .expected_tree = output::Hole{}
    },
    {
      .str = "\\a.b",
      .expected_tree = output::Lambda{
        .body = output::Identifier{"b"},
        .arg_name = "a"
      }
    },
    {
      .str = "\\.b",
      .expected_tree = output::Lambda{
        .body = output::Identifier{"b"},
      }
    },
    {
      .str = "\\:a.b",
      .expected_tree = output::Lambda{
        .body = output::Identifier{"b"},
        .type = output::Identifier{"a"}
      }
    },
    {
      .str = "\\x:a.b",
      .expected_tree = output::Lambda{
        .body = output::Identifier{"b"},
        .type = output::Identifier{"a"},
        .arg_name = "x"
      }
    },
    {
      .str = "a->b",
      .expected_tree = output::Arrow{
        .domain = output::Identifier{"a"},
        .codomain = output::Identifier{"b"}
      }
    },
    {
      .str = "(x:a)->b",
      .expected_tree = output::Arrow{
        .domain = output::Identifier{"a"},
        .codomain = output::Identifier{"b"},
        .arg_name = "x"
      }
    },
    {
      .str = " ( x : a ) -> b",
      .expected_tree = output::Arrow{
        .domain = output::Identifier{"a"},
        .codomain = output::Identifier{"b"},
        .arg_name = "x"
      }
    },
    {
      .str = " x ",
      .expected_tree = output::Identifier{"x"}
    },
    {
      .str = "(x)",
      .expected_tree = output::Identifier{"x"}
    },
    {
      .str = " ( x ) ",
      .expected_tree = output::Identifier{"x"}
    },
    {
      .str = "block { f x = x; f }",
      .expected_tree = output::Block {
        .statements = {
          output::Rule{
            .pattern = output::PatternApply{
              .lhs = output::PatternIdentifier{"f"},
              .rhs = output::PatternIdentifier{"x"}
            },
            .replacement = output::Identifier{"x"}
          }
        },
        .value = output::Identifier{"f"}
      }
    },
    {
      .str = "block { f x where x = ax y = y; f }",
      .expected_tree = output::Block {
        .statements = {
          output::Rule{
            .pattern = output::PatternApply{
              .lhs = output::PatternIdentifier{"f"},
              .rhs = output::PatternIdentifier{"x"}
            },
            .subclause_expressions = {
              output::Identifier{"x"}
            },
            .subclause_patterns = {
              output::PatternApply{
                .lhs = output::PatternIdentifier{"ax"},
                .rhs = output::PatternIdentifier{"y"}
              }
            },
            .replacement = output::Identifier{"y"}
          }
        },
        .value = output::Identifier{"f"}
      }
    },
    {
      .str = "block { f x where x = ax y where g x y = axx z w = w; f }",
      .expected_tree = output::Block {
        .statements = {
          output::Rule{
            .pattern = output::PatternApply{
              .lhs = output::PatternIdentifier{"f"},
              .rhs = output::PatternIdentifier{"x"}
            },
            .subclause_expressions = {
              output::Identifier{"x"},
              output::Apply{
                output::Apply{
                  output::Identifier{"g"},
                  output::Identifier{"x"}
                },
                output::Identifier{"y"}
              }
            },
            .subclause_patterns = {
              output::PatternApply{
                .lhs = output::PatternIdentifier{"ax"},
                .rhs = output::PatternIdentifier{"y"}
              },
              output::PatternApply{
                .lhs = output::PatternApply{
                  .lhs = output::PatternIdentifier{"axx"},
                  .rhs = output::PatternIdentifier{"z"}
                },
                .rhs = output::PatternIdentifier{"w"}
              }
            },
            .replacement = output::Identifier{"w"}
          }
        },
        .value = output::Identifier{"f"}
      }
    }
  };
  auto format_stream = mdb::overloaded{
    [](auto& o, std::optional<std::string_view> const& v) { if(v) o << "\"" << *v << "\""; else o << "none"; },
    [](auto& o, std::string_view const& v) { o << "\"" << v << "\""; },
    [](auto& o, auto&&) { o << "???"; }
  };
  expression_parser::LexerInfo lexer_info {
    .symbol_map = {
      {"block", 0},
      {"declare", 1},
      {"axiom", 2},
      {"rule", 3},
      {"let", 4},
      {"->", 5},
      {":", 6},
      {";", 7},
      {"=", 8},
      {"\\", 9},
      {"\\\\", 10},
      {".", 11},
      {"_", 12},
      {",", 13},
      {"where", 14}
    }
  };
  for(auto const& test : test_cases) {
    INFO("String: \"" << test.str << "\"");
    INFO("Expected tree: " << format(test.expected_tree, format_stream));

    auto lex = expression_parser::lex_string(test.str, lexer_info);
    if(auto* err = lex.get_if_error()) {
      FAIL("Lex Error: " << err->message << "\nAt: " << err->position);
    }
    auto lex_out = archive(lex.get_value().output);
    auto ret = expression_parser::parse_lexed(lex_out);
    if(auto* error = ret.get_if_error()) {
      FAIL("Failed to parse: " << error->message); //TODO: trace back through lexer
    } else {
      auto& success = ret.get_value();
      INFO("Received tree: " << format(success.output, format_stream));
      REQUIRE(success.output == test.expected_tree);
    }
  }
}
