#include "../CombinatorComputation/matching.hpp"
#include <catch.hpp>

using namespace combinator;

TEST_CASE("Matches are correctly identified."){
  auto term = Term{
    Term{TermLeaf{0}},
    Term{TermLeaf{1}}
  };

  auto pattern_1 = Pattern{
    Pattern{PatternLeaf{0}},
    Pattern{PatternLeaf{}}
  };

  auto pattern_2 = Pattern{
    Pattern{PatternLeaf{}}
  };

  auto pattern_3 = Pattern{
    Pattern{PatternLeaf{1}},
    Pattern{PatternLeaf{}}
  };

  auto pattern_4 = Pattern{
    Pattern{PatternLeaf{}},
    Pattern{PatternLeaf{1}}
  };

  auto pattern_5 = Pattern{
    Pattern{PatternLeaf{}},
    Pattern{PatternLeaf{2}}
  };


  REQUIRE(term_matches(term, pattern_1));
  REQUIRE(term_matches(term, pattern_2));
  REQUIRE_FALSE(term_matches(term, pattern_3));
  REQUIRE(term_matches(term, pattern_4));
  REQUIRE_FALSE(term_matches(term, pattern_5));
}

TEST_CASE("Matches can be found in larger expressions.") {
  auto term = Term{
    Term{
      Term{TermLeaf{0}},
      Term{TermLeaf{1}}
    },
    Term{
      Term{TermLeaf{0}},
      Term{TermLeaf{2}}
    }
  };

  auto pattern_1 = Pattern{
    Pattern{PatternLeaf{0}},
    Pattern{PatternLeaf{}}
  };

  auto matches = find_all_matches(term, pattern_1);

  REQUIRE(matches.size() == 2);
  REQUIRE(matches[0].steps == std::vector<mdb::TreeStep>{mdb::TreeStep::left});
  REQUIRE(matches[1].steps == std::vector<mdb::TreeStep>{mdb::TreeStep::right});

  auto pattern_2 = Pattern{
    Pattern{PatternLeaf{}},
    Pattern{PatternLeaf{}}
  };

  matches = find_all_matches(term, pattern_2);

  REQUIRE(matches.size() == 3);
  REQUIRE(matches[0].steps == std::vector<mdb::TreeStep>{});
  REQUIRE(matches[1].steps == std::vector<mdb::TreeStep>{mdb::TreeStep::left});
  REQUIRE(matches[2].steps == std::vector<mdb::TreeStep>{mdb::TreeStep::right});

  auto pattern_3 = Pattern{
    Pattern{PatternLeaf{}},
    Pattern{PatternLeaf{2}}
  };

  matches = find_all_matches(term, pattern_3);

  REQUIRE(matches.size() == 1);
  REQUIRE(matches[0].steps == std::vector<mdb::TreeStep>{mdb::TreeStep::right});

  auto pattern_4 = Pattern{
    Pattern{PatternLeaf{}},
    Pattern{
      Pattern{PatternLeaf{1}},
      Pattern{PatternLeaf{}}
    }
  };

  matches = find_all_matches(term, pattern_4);

  REQUIRE(matches.empty());

}

TEST_CASE("Matches can be properly destructured.") {

  auto term = Term{
    Term{
      Term{TermLeaf{0}},
      Term{TermLeaf{1}}
    },
    Term{
      Term{TermLeaf{0}},
      Term{TermLeaf{2}}
    }
  };

  auto pattern = Pattern{
    Pattern{
      Pattern{PatternLeaf{0}},
      Pattern{PatternLeaf{}}
    },
    Pattern{PatternLeaf{}}
  };

  REQUIRE(term_matches(term, pattern));
  auto captures = destructure_match(term, pattern);

  REQUIRE(captures.size() == 2);
  REQUIRE(captures[0] == Term{TermLeaf{1}});
  REQUIRE(captures[1] == Term{
    Term{TermLeaf{0}},
    Term{TermLeaf{2}}
  });

}

TEST_CASE("Substitution into replacements works properly.") {

  auto replacement = Replacement{
    constant_replacement(20),
    Replacement{
      arg_replacement(0),
      Replacement{
        arg_replacement(0),
        arg_replacement(1)
      }
    }
  };

  std::vector<Term> captures = {
    Term{
      Term{TermLeaf{10}},
      Term{TermLeaf{11}}
    },
    Term{TermLeaf{12}}
  };

  auto ret = substitute_into_replacement(captures, std::move(replacement));

  REQUIRE(ret == Term{
    Term{TermLeaf{20}},
    Term{
      Term{
        Term{TermLeaf{10}},
        Term{TermLeaf{11}}
      },
      Term{
        Term{
          Term{TermLeaf{10}},
          Term{TermLeaf{11}}
        },
        Term{TermLeaf{12}}
      }
    }
  });

}

TEST_CASE("Pattern matching and substituting into deep replacements works properly.") {

  auto term = Term{
    Term{
      Term{
        Term{TermLeaf{10}},
        Term{TermLeaf{11}},
      },
      Term{
        Term{TermLeaf{12}},
        Term{TermLeaf{13}}
      }
    },
    Term{TermLeaf{14}}
  };

  auto replacement = Replacement{
    arg_replacement(1),
    constant_replacement(20)
  };

  auto pattern = Pattern{
    Pattern{
      Pattern{PatternLeaf{10}},
      Pattern{PatternLeaf{}}
    },
    Pattern{PatternLeaf{}}
  };

  auto replaced = make_substitution(std::move(term), {{mdb::TreeStep::left}}, pattern, replacement);

  REQUIRE(replaced == Term{
    Term{
      Term{
        Term{TermLeaf{12}},
        Term{TermLeaf{13}}
      },
      Term{TermLeaf{20}}
    },
    Term{TermLeaf{14}}
  });

}
