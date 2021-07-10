#include "../Parser/parser.hpp"
#include <catch.hpp>

using namespace parser;

TEST_CASE("Basic parser pieces work."){

  constexpr auto is_x = char_predicate([](char c){ return c == 'x' || c == 'X'; });
  REQUIRE(holds_success(is_x("xy")));
  REQUIRE_FALSE(holds_success(is_x("yx")));
  REQUIRE_FALSE(holds_success(is_x("")));

  constexpr auto is_y = char_predicate([](char c){ return c == 'y' || c == 'Y'; });

  constexpr auto is_xy = sequence(is_x, is_y);
  REQUIRE(holds_success(is_xy("xy")));
  REQUIRE(holds_success(is_xy("xY")));
  REQUIRE(holds_success(is_xy("Xy")));
  REQUIRE(holds_success(is_xy("XY")));

  {
    auto ret = get_success(is_xy("XY")).value;
    REQUIRE(std::get<0>(ret) == 'X');
    REQUIRE(std::get<1>(ret) == 'Y');
  }

  REQUIRE_FALSE(holds_success(is_xy("xX")));
  REQUIRE_FALSE(holds_success(is_xy("yy")));
  REQUIRE_FALSE(holds_success(is_xy("xZ")));
  REQUIRE_FALSE(holds_success(is_xy("x")));
  REQUIRE_FALSE(holds_success(is_xy("")));

  constexpr auto is_z = char_predicate([](char c){ return c == 'z' || c == 'Z'; });
  constexpr auto branched = simple_condition_branch(is_z, sequence(is_z, is_x), sequence(is_x, is_y));

  REQUIRE(holds_success(branched("zx")));
  REQUIRE(holds_success(branched("xy")));
  REQUIRE_FALSE(holds_success(branched("zy")));
  REQUIRE_FALSE(holds_success(branched("xx")));

  constexpr auto is_capital = char_predicate([](char c) { return c == 'X' || c == 'Y' || c == 'Z'; });

  constexpr auto branched_multi = branch(
    std::make_pair(is_z, sequence(is_z, is_x)),
    std::make_pair(is_y, sequence(is_y, is_x)),
    std::make_pair(is_capital, sequence(is_capital, is_capital))
  );

  REQUIRE(holds_success(branched_multi("zx")));
  REQUIRE(holds_success(branched_multi("YX")));
  REQUIRE(holds_success(branched_multi("XY")));
  REQUIRE_FALSE(holds_success(branched_multi("zY")));
  REQUIRE_FALSE(holds_success(branched_multi("Yy")));
  REQUIRE_FALSE(holds_success(branched_multi("xx")));

  constexpr auto hello = symbol("hello");
  constexpr auto world = symbol("world");
  constexpr auto helloworld = sequence(hello, world);

  REQUIRE(holds_success(helloworld("helloworld")));
  REQUIRE_FALSE(holds_success(helloworld("hellohelloworld")));
  REQUIRE_FALSE(holds_success(helloworld("hello")));

}

  //Match matched parenthesis
  /*
  balanced = simple_condition_branch(is_open_paren,
    sequence(is_open_paren, balanced, is_close_paren),
    always_match
  )
  */

#include "../Parser/recursive_macros.hpp"


namespace test {
  constexpr auto is_open_paren = symbol("(");
  constexpr auto is_close_paren = symbol(")");

  MB_DECLARE_RECURSIVE_PARSER(nested_parenthesis, parser::Empty);
  MB_DEFINE_RECURSIVE_PARSER(nested_parenthesis,
    simple_condition_branch(is_open_paren,
      sequence(is_open_paren, nested_parenthesis, is_close_paren),
      always_match
    )
  );
}

TEST_CASE("Recursive parsers can be defined.") {
  REQUIRE(holds_success(test::nested_parenthesis("")));
  REQUIRE(holds_success(test::nested_parenthesis("()")));
  REQUIRE(holds_success(test::nested_parenthesis("(())")));
  REQUIRE_FALSE(holds_success(test::nested_parenthesis("((")));
  REQUIRE_FALSE(holds_success(test::nested_parenthesis("((())")));
}

namespace calculator {
  constexpr auto open_paren = symbol("(");
  constexpr auto close_paren = symbol(")");
  constexpr auto op = char_predicate([](char c) { return c == '+' || c == '*'; });
  constexpr auto literal = integer<int>;

  /*
    5 (extensions)
  */

  MB_DECLARE_RECURSIVE_PARSER(expression, int);

  constexpr auto term = simple_condition_branch(open_paren,
    sequence(open_paren, expression, close_paren),
    literal
  );

  MB_DEFINE_RECURSIVE_PARSER(expression,
    map_tuple(sequence(term, op, term), [](int x, char op, int y) {
      if(op == '+') {
        return x + y;
      } else {
        return x * y;
      }
    })
  );
}

TEST_CASE("Calculator works.") {
  std::pair<const char*, int> success_cases[] = {
    {"5", 5},
    {"(2+3)",5},
    {"((3*5)+(1*2))", 17},
    {"(((((1+2)+3)+4)+5)+6)", 21}
  };
  for(auto const& [str, val] : success_cases) {
    auto ret = calculator::term(str);
    REQUIRE(holds_success(ret));
    auto const& success = get_success(ret);
    REQUIRE(success.remaining.empty());
    REQUIRE(success.value == val);
  }
}
