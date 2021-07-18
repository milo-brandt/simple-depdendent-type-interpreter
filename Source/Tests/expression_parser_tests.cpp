#include "../ExpressionParser/expression_parser.hpp"
#include <catch.hpp>

using namespace expression_parser;

TEST_CASE("The expression parser matches various expressions.") {
  struct Test {
    std::string_view str;
    output::Tree expected_tree;
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
    }
  };
  auto format_stream = mdb::overloaded{
    [](auto& o, std::optional<std::string_view> const& v) { if(v) o << "\"" << *v << "\""; else o << "none"; },
    [](auto& o, std::string_view const& v) { o << "\"" << v << "\""; }
  };
  for(auto const& test : test_cases) {
    INFO("String: \"" << test.str << "\"");
    INFO("Expected tree: " << (output::FormatTree{test.expected_tree, format_stream}));

    auto ret = expression_parser::parser::expression(test.str);
    if(auto* error = expression_parser::parser::get_if_error(&ret)) {
      FAIL("Failed to parse: " << error->error << "\nAt char: " << (error->position.begin() - test.str.begin()));
    } else {
      auto& success = get_success(ret);
      INFO("Received tree: " << (output::FormatTree{success.value.output, format_stream}));
      REQUIRE(success.value.output == test.expected_tree);
    }
  }
}
