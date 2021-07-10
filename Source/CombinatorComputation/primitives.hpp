#ifndef COMBINATOR_COMPUTATION_PRIMITIVES_HPP
#define COMBINATOR_COMPUTATION_PRIMITIVES_HPP

#include "../Utility/binary_tree.hpp"
#include <variant>
#include <cstdint>
#include <limits>

namespace combinator {
  struct TermLeaf {
    std::uint64_t index;
  };
  constexpr bool operator==(TermLeaf const& lhs, TermLeaf const& rhs) { return lhs.index == rhs.index; }
  using Term = mdb::BinaryTree<TermLeaf>;
  struct PatternLeaf {
    std::uint64_t data;
    PatternLeaf():data{std::numeric_limits<std::uint64_t>::max()}{}
    explicit PatternLeaf(std::uint64_t data):data(data){}
    operator bool() const { return data != std::numeric_limits<std::uint64_t>::max(); }
    std::uint64_t operator*() const { assert(*this); return data; }
  };
  constexpr bool operator==(PatternLeaf const& lhs, PatternLeaf const& rhs) { return lhs.data == rhs.data; }
  using Pattern = mdb::BinaryTree<PatternLeaf>;
  struct ReplacementLeaf {
    bool constant;
    std::uint64_t index;
  };
  constexpr bool operator==(ReplacementLeaf const& lhs, ReplacementLeaf const& rhs) { return lhs.constant == rhs.constant && lhs.index == rhs.index; }
  using Replacement = mdb::BinaryTree<ReplacementLeaf>;
  inline Replacement constant_replacement(std::uint64_t index) { return Replacement{ReplacementLeaf{true, index}}; }
  inline Replacement arg_replacement(std::uint64_t index) { return Replacement{ReplacementLeaf{false, index}}; }
  struct ReplacementRule {
    Pattern pattern;
    Replacement replacement;
  };
}

#endif
