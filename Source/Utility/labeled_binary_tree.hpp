#include "binary_tree.hpp"

//mostly copied from binary_tree.hpp, mutatis mutandi

namespace mdb {
  /*
    Declaration of binary trees
  */
  template<class Leaf, class Label = Leaf>
  class LabeledBinaryTree {
    std::variant<Leaf, Indirect<std::tuple<Label, LabeledBinaryTree, LabeledBinaryTree> > > data;
  public:
    explicit LabeledBinaryTree(Leaf leaf):data(std::in_place_index<0>, std::move(leaf)) {}
    LabeledBinaryTree(Label label, LabeledBinaryTree lhs, LabeledBinaryTree rhs):data(std::in_place_index<1>, mdb::in_place, std::move(label), std::move(lhs), std::move(rhs)) {}

    Leaf* get_if_leaf() { return std::get_if<0>(&data); }
    Leaf const* get_if_leaf() const { return std::get_if<0>(&data); }
    std::tuple<Label, LabeledBinaryTree, LabeledBinaryTree>* get_if_node() {
      if(auto* indirect = std::get_if<1>(&data)) {
        return indirect->get();
      } else {
        return nullptr;
      }
    }
    std::tuple<Label, LabeledBinaryTree, LabeledBinaryTree> const* get_if_node() const {
      if(auto* indirect = std::get_if<1>(&data)) {
        return indirect->get();
      } else {
        return nullptr;
      }
    }

    Leaf& get_leaf() {
      auto ret = get_if_leaf();
      assert(ret);
      return *ret;
    }
    Leaf const& get_leaf() const {
      auto ret = get_if_leaf();
      assert(ret);
      return *ret;
    }
    std::tuple<Label, LabeledBinaryTree, LabeledBinaryTree>& get_node() {
      auto ret = get_if_node();
      assert(ret);
      return *ret;
    }
    std::tuple<Label, LabeledBinaryTree, LabeledBinaryTree> const& get_node() const {
      auto ret = get_if_node();
      assert(ret);
      return *ret;
    }
    template<class F, class G>
    decltype(auto) visit(F&& if_leaf, G&& if_node) {
      if(data.index() == 0) {
        return if_leaf(std::get<0>(data));
      } else {
        auto& [label, lhs, rhs] = *std::get<1>(data);
        return if_node(label, lhs, rhs);
      }
    }
    template<class F, class G>
    decltype(auto) visit(F&& if_leaf, G&& if_node) const {
      if(data.index() == 0) {
        return if_leaf(std::get<0>(data));
      } else {
        auto const& [label, lhs, rhs] = *std::get<1>(data);
        return if_node(label, lhs, rhs);
      }
    }
  };

  template<class Leaf, class Label> Leaf* get_if_leaf(LabeledBinaryTree<Leaf, Label>* candidate) { if(candidate) return candidate->get_if_leaf(); else return nullptr; }
  template<class Leaf, class Label> Leaf const* get_if_leaf(LabeledBinaryTree<Leaf, Label> const* candidate) { if(candidate) return candidate->get_if_leaf(); else return nullptr; }
  template<class Leaf, class Label> std::tuple<Label, LabeledBinaryTree<Leaf, Label>, LabeledBinaryTree<Leaf, Label> >* get_if_node(BinaryTree<Leaf>* candidate) { if(candidate) return candidate->get_if_node(); else return nullptr; }
  template<class Leaf, class Label> std::tuple<Label, LabeledBinaryTree<Leaf, Label>, LabeledBinaryTree<Leaf, Label> > const* get_if_node(BinaryTree<Leaf> const* candidate) { if(candidate) return candidate->get_if_node(); else return nullptr; }

  template<class Leaf, class Label, class F, class G, class T = std::invoke_result_t<F, Leaf const&> >
  T recurse_tree(LabeledBinaryTree<Leaf, Label> const& tree, F&& leaf_handler, G&& node_handler) {
    return tree.visit(
      [&](Leaf const& leaf) {
        return leaf_handler(leaf);
      },
      [&](Label const& label, LabeledBinaryTree<Leaf, Label> const& lhs, LabeledBinaryTree<Leaf, Label> const& rhs) {
        return node_handler(label, recurse_tree(lhs, leaf_handler, node_handler), recurse_tree(rhs, leaf_handler, node_handler));
      });
  }
  template<class Leaf, class Label, class F, class G, class T = std::invoke_result_t<F, Leaf&&> >
  T recurse_tree(LabeledBinaryTree<Leaf, Label>&& tree, F&& leaf_handler, G&& node_handler) {
    return tree.visit(
      [&](Leaf& leaf) {
        return leaf_handler(std::move(leaf));
      },
      [&](Label& label, LabeledBinaryTree<Leaf, Label>& lhs, LabeledBinaryTree<Leaf, Label>& rhs) {
        return node_handler(std::move(label), recurse_tree(std::move(lhs), leaf_handler, node_handler), recurse_tree(std::move(rhs), leaf_handler, node_handler));
      });
  }
  template<class Leaf, class Label>
  bool operator==(LabeledBinaryTree<Leaf, Label> const& me, LabeledBinaryTree<Leaf, Label> const& other) {
    return me.visit(
      [&](Leaf const& leaf) {
        if(auto* other_leaf = other.get_if_leaf()) {
          return leaf == *other_leaf;
        } else {
          return false;
        }
      },
      [&](Label const& label, LabeledBinaryTree<Leaf, Label> const& lhs, LabeledBinaryTree<Leaf, Label> const& rhs) {
        if(auto* other_node = other.get_if_node()) {
          return label == std::get<0>(*other_node) && lhs == std::get<1>(*other_node) && rhs == std::get<2>(*other_node);
        } else {
          return false;
        }
      });
  }
  /*
    If Leaf = Label
  */
  template<class L>
  L const& read_tree_label(LabeledBinaryTree<L, L> const& tree) {
    return tree.visit(
      [](L const& label) -> L const& {
        return label;
      },
      [](L const& label, auto const&, auto const&) -> L const& {
        return label;
      }
    );
  }
  template<class L>
  L&& read_tree_label(LabeledBinaryTree<L, L>&& tree) {
    return tree.visit(
      [](L&& label) -> L&& {
        return label;
      },
      [](L&& label, auto const&, auto const&) -> L&& {
        return label;
      }
    );
  }

  /*
    Zippers
  */
  template<class Leaf, class Label>
  LabeledBinaryTree<Leaf, Label> step_tree_by(LabeledBinaryTree<Leaf, Label>&& tree, TreeStep step) {
    if(auto* pair = tree.get_if_node()) {
      if(step == TreeStep::left) {
        return std::move(std::get<1>(*pair));
      } else {
        return std::move(std::get<2>(*pair));
      }
    } else {
      throw UnzipException{};
    }
  }
  template<class Leaf, class Label>
  LabeledBinaryTree<Leaf, Label> const& step_tree_by(LabeledBinaryTree<Leaf, Label> const& tree, TreeStep step) {
    if(auto* pair = tree.get_if_node()) {
      if(step == TreeStep::left) {
        return std::get<1>(*pair);
      } else {
        return std::get<2>(*pair);
      }
    } else {
      throw UnzipException{};
    }
  }

  template<class Leaf, class Label>
  LabeledBinaryTree<Leaf, Label> tree_at_path(LabeledBinaryTree<Leaf, Label>&& tree, TreePath const& path) {
    return std::accumulate(path.steps.begin(), path.steps.end(), std::move(tree), [](auto tree, TreeStep step){ return step_tree_by(tree, step); });
  }
  template<class Leaf, class Label>
  LabeledBinaryTree<Leaf, Label> const& tree_at_path(LabeledBinaryTree<Leaf, Label> const& tree, TreePath const& path) {
    return *std::accumulate(path.steps.begin(), path.steps.end(), &tree, [](auto const* tree, TreeStep step) -> LabeledBinaryTree<Leaf, Label> const* { return &step_tree_by(*tree, step); });
  }



}
