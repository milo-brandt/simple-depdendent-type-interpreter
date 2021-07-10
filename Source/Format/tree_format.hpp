#ifndef TREE_FORMAT_HPP
#define TREE_FORMAT_HPP

#include <sstream>
#include <type_traits>
#include "../Utility/binary_tree.hpp"
#include "../Utility/labeled_binary_tree.hpp"

namespace format {
  struct Span {
    std::size_t start;
    std::size_t end;
  };
  struct FormattedTree {
    std::string result;
    mdb::LabeledBinaryTree<Span> spans;
  };
  template<class Leaf, class LeafFormat> requires (std::is_invocable_v<LeafFormat, std::stringstream&, Leaf const&>)
  FormattedTree format_expression_tree(mdb::BinaryTree<Leaf> const& tree, LeafFormat&& format_leaf) {
    struct Impl {
      std::stringstream out;
      LeafFormat& format_leaf;
      mdb::LabeledBinaryTree<Span> print(mdb::BinaryTree<Leaf> const& tree, bool parenthesize_applications) {
        return tree.visit(
          [&](Leaf const& leaf) {
            std::uint64_t start_pos = out.tellp();
            format_leaf(out, leaf);
            std::uint64_t end_pos = out.tellp();
            return mdb::LabeledBinaryTree<Span>{Span{start_pos, end_pos}};
          },
          [&](mdb::BinaryTree<Leaf> const& lhs, mdb::BinaryTree<Leaf> const& rhs) {
            if(parenthesize_applications) {
              out << "(";
            }
            std::uint64_t start_pos = out.tellp();
            auto lhs_pos = print(lhs, false);
            out << " ";
            auto rhs_pos = print(rhs, true);
            std::uint64_t end_pos = out.tellp();
            if(parenthesize_applications) {
              out << ")";
            }
            return mdb::LabeledBinaryTree<Span>{
              Span{start_pos, end_pos},
              std::move(lhs_pos),
              std::move(rhs_pos)
            };
          }
        );
      }
    };
    Impl impl{
      .format_leaf = format_leaf
    };
    auto positions = impl.print(tree, false);
    return {
      .result = impl.out.str(),
      .spans = std::move(positions)
    };
  }
}

#endif
