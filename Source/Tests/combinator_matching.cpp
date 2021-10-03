/*

#include "../Expression/expression_tree.hpp"
#include <catch.hpp>

using namespace expression;
using namespace expression::tree;

TEST_CASE("Matches are correctly identified."){
  auto term = Apply{
    External{0},
    External{1}
  };

  auto pattern_1 = pattern::Apply{
    pattern::Fixed{0},
    pattern::Wildcard{}
  };

  auto pattern_2 = pattern::Wildcard{};

  auto pattern_3 = pattern::Apply{
    pattern::Fixed{1},
    pattern::Wildcard{}
  };

  auto pattern_4 = pattern::Apply{
    pattern::Wildcard{},
    pattern::Fixed{1}
  };

  auto pattern_5 = pattern::Apply{
    pattern::Wildcard{},
    pattern::Fixed{2}
  };


  REQUIRE(term_matches(term, pattern_1));
  REQUIRE(term_matches(term, pattern_2));
  REQUIRE_FALSE(term_matches(term, pattern_3));
  REQUIRE(term_matches(term, pattern_4));
  REQUIRE_FALSE(term_matches(term, pattern_5));
}

TEST_CASE("Matches can be found in larger expressions.") {
  Expression term = Apply{
    Apply{
      External{0},
      External{1}
    },
    Apply{
      External{0},
      External{2}
    }
  };

  auto pattern_1 = pattern::Apply{
    pattern::Fixed{0},
    pattern::Wildcard{}
  };

  auto matches = find_all_matches(term, pattern_1);

  REQUIRE(matches.size() == 2);
  REQUIRE(matches[0] == &term.get_apply().lhs);
  REQUIRE(matches[1] == &term.get_apply().rhs);

  auto pattern_2 = pattern::Apply{
    pattern::Wildcard{},
    pattern::Wildcard{}
  };

  matches = find_all_matches(term, pattern_2);

  REQUIRE(matches.size() == 3);
  REQUIRE(matches[0] == &term);
  REQUIRE(matches[1] == &term.get_apply().lhs);
  REQUIRE(matches[2] == &term.get_apply().rhs);

  auto pattern_3 = pattern::Apply{
    pattern::Wildcard{},
    pattern::Fixed{2}
  };

  matches = find_all_matches(term, pattern_3);

  REQUIRE(matches.size() == 1);
  REQUIRE(matches[0] == &term.get_apply().rhs);

  auto pattern_4 = pattern::Apply{
    pattern::Wildcard{},
    pattern::Apply{
      pattern::Fixed{1},
      pattern::Wildcard{}
    }
  };

  matches = find_all_matches(term, pattern_4);

  REQUIRE(matches.empty());

}

TEST_CASE("Matches can be properly destructured.") {

  auto term = Apply{
    Apply{
      External{0},
      External{1}
    },
    Apply{
      External{0},
      External{2}
    }
  };

  auto pattern = pattern::Apply{
    pattern::Apply{
      pattern::Fixed{0},
      pattern::Wildcard{}
    },
    pattern::Wildcard{}
  };

  REQUIRE(term_matches(term, pattern));
  auto captures = destructure_match(term, pattern);

  REQUIRE(captures.size() == 2);
  REQUIRE(captures[0] == External{1});
  REQUIRE(captures[1] == Apply{
    External{0},
    External{2}
  });

}

TEST_CASE("Substitution into replacements works properly.") {

  auto replacement = Apply{
    External{20},
    Apply{
      Arg{0},
      Apply{
        Arg{0},
        Arg{1}
      }
    }
  };

  std::vector<Expression> captures = {
    Apply{
      External{10},
      External{11}
    },
    External{12}
  };

  auto ret = substitute_into_replacement(captures, std::move(replacement));

  REQUIRE(ret == Apply{
    External{20},
    Apply{
      Apply{
        External{10},
        External{11}
      },
      Apply{
        Apply{
          External{10},
          External{11}
        },
        External{12}
      }
    }
  });
}

TEST_CASE("Pattern matching and substituting into deep replacements works properly.") {

  Expression term = Apply{
    Apply{
      Apply{
        External{10},
        External{11}
      },
      Apply{
        External{12},
        External{13}
      }
    },
    External{14}
  };

  auto replacement = Apply{
    Arg{1},
    External{20}
  };

  auto pattern = pattern::Apply{
    pattern::Apply{
      pattern::Fixed{10},
      pattern::Wildcard{}
    },
    pattern::Wildcard{}
  };

  replace_with_substitution_at(&term.get_apply().lhs, pattern, replacement);

  REQUIRE(term == Apply{
    Apply{
      Apply{
        External{12},
        External{13}
      },
      External{20}
    },
    External{14}
  });

}

*/
