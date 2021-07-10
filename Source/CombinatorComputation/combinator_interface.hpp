#ifndef COMBINATOR_COMPUTATION_INTERFACE_HPP
#define COMBINATOR_COMPUTATION_INTERFACE_HPP

#include "primitives.hpp"
#include "../Format/tree_format.hpp"
#include "../Utility/function.hpp"

namespace combinator {
  struct Match {
    format::Span trigger;
    format::Span area;
    std::vector<format::Span> matches;
  };
  struct InterfaceOutput {
    std::string string;
    std::vector<Match> matches;
  };
  struct InterfaceState {
    std::vector<Term> updates;
  };
  std::pair<InterfaceOutput, InterfaceState> process_term(Term const& term, std::vector<ReplacementRule> const& rules, mdb::function<void(std::stringstream&, TermLeaf const)> format_leaf);
};

#endif
