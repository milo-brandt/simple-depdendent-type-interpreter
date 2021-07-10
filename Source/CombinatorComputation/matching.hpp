#ifndef COMBINATOR_COMPUTATION_MATCHING_HPP
#define COMBINATOR_COMPUTATION_MATCHING_HPP

#include "primitives.hpp"

namespace combinator {
  bool term_matches(Term const&, Pattern const&);
  struct NoMatchException : std::runtime_error {
    NoMatchException():std::runtime_error("Attempted to destructure non-existant match.") {}
  };
  std::vector<Term> destructure_match(Term, Pattern const&); //throws if not matching
  std::vector<mdb::TreePath> captures_of_pattern(Pattern const&);
  std::vector<mdb::TreePath> find_all_matches(Term const&, Pattern const&);

  struct NotEnoughArguments : std::runtime_error {
    NotEnoughArguments():std::runtime_error("Not enough arguments to substitute into replacement.") {}
  };

  Term substitute_into_replacement(std::vector<Term> const& terms, Replacement const&);
  Term make_substitution(Term, mdb::TreePath const&, Pattern const&, Replacement const&);
}

#endif
