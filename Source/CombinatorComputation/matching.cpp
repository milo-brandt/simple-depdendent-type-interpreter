#include "matching.hpp"

namespace combinator {
  bool term_matches(Term const& term, Pattern const& p) {
    struct Impl {
      bool visit(Term const& position, Pattern const& p) {
        return p.visit([&](PatternLeaf const& leaf) {
          if(!leaf) {
            return true;
          } else {
            if(auto* term_leaf = position.get_if_leaf()) {
              return term_leaf->index == *leaf;
            } else {
              return false;
            }
          }
        }, [&](mdb::BinaryTree<PatternLeaf> const& lhs, mdb::BinaryTree<PatternLeaf> const& rhs) {
          if(auto* term_node = position.get_if_node()) {
            return visit(term_node->first, lhs) && visit(term_node->second, rhs);
          } else {
            return false;
          }
        });
      }
    };
    return Impl{}.visit(term, p);
  }
  std::vector<Term> destructure_match(Term term, Pattern const& p) {
    struct Impl {
      std::vector<Term> output;
      void visit(Term&& position, Pattern const& p) {
        p.visit([&](PatternLeaf const& leaf) {
          if(!leaf) {
            output.push_back(Term{std::move(position)});
          } else {
            if(auto* term_leaf = position.get_if_leaf()) {
              if(term_leaf->index != *leaf) {
                throw NoMatchException{}; //required leaf does not match
              }
            } else {
              throw NoMatchException{}; //node where leaf required
            }
          }
        }, [&](mdb::BinaryTree<PatternLeaf> const& lhs, mdb::BinaryTree<PatternLeaf> const& rhs) {
          if(auto* term_node = position.get_if_node()) {
            visit(std::move(term_node->first), lhs);
            visit(std::move(term_node->second), rhs);
          } else {
            throw NoMatchException{}; //leaf where node required
          }
        });
      }
    };
    Impl helper{};
    helper.visit(std::move(term), p);
    return std::move(helper.output);
  }
  std::vector<mdb::TreePath> captures_of_pattern(Pattern const& pattern) {
    std::vector<mdb::TreePath> captures;
    for(auto const& [path, t] : mdb::recursive_range_of(pattern)) {
      if(auto* node = t.get_if_leaf()) {
        if(!*node) {
          captures.push_back(path);
        }
      }
    }
    return captures;
  }
  std::vector<mdb::TreePath> find_all_matches(Term const& term, Pattern const& pattern) {
    std::vector<mdb::TreePath> ret;
    for(auto const& [path, t] : mdb::recursive_range_of(term)) {
      if(term_matches(t, pattern)) {
        ret.push_back(path);
      }
    }
    return ret;
  }

  Term substitute_into_replacement(std::vector<Term> const& terms, Replacement const& replacement) {
    return mdb::bind_tree(replacement, [&](ReplacementLeaf leaf) {
      if(leaf.constant) {
        return Term{TermLeaf{leaf.index}};
      } else {
        if(leaf.index < terms.size()) {
          return terms[leaf.index];
        } else {
          throw NotEnoughArguments{};
        }
      }
    });
  }

  Term make_substitution(Term term, mdb::TreePath const& path, Pattern const& pattern, Replacement const& replacement) {
    auto zipper = mdb::unzip_tree(std::move(term), path);
    auto matches = destructure_match(std::move(zipper.head), pattern);
    zipper.head = substitute_into_replacement(matches, replacement);
    return std::move(zipper).zip();
  }
}
