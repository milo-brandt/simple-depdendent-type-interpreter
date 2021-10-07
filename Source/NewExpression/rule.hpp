#ifndef EXPRESSION_RULE_HPP
#define EXPRESSION_RULE_HPP

#include "arena.hpp"
#include "arena_container.hpp"
#include "on_arena_utility.hpp"
#include <vector>

namespace new_expression {
  //may eventually want to make a "parts" metaprogramming library to auto-generate
  //"on-arena" destructors for these types.
  struct PatternMatch {
    OwnedExpression substitution;
    OwnedExpression expected_head;
    std::size_t args_captured;
    static constexpr auto part_info = mdb::parts::simple<3>;
  };
  struct DataChecks {
    std::size_t capture_index;
    std::uint64_t expected_type;
  };
  struct PatternBody {
    std::size_t args_captured;
    std::vector<PatternMatch> sub_matches;
    std::vector<DataChecks> data_checks;
    static constexpr auto part_info = mdb::parts::simple<3>;
  }; //output is some list of captures, if successful
  struct Pattern {
    OwnedExpression head;
    PatternBody body;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };
  struct Rule {
    Pattern pattern;
    OwnedExpression replacement;
    static constexpr auto part_info = mdb::parts::simple<2>;
  };

}

#endif
