#ifndef BINARY_TREE_HPP
#define BINARY_TREE_HPP

#include "tags.hpp"
#include "indirect.hpp"
#include <variant>
#include <cassert>
#include <vector>
#include <exception>
#include <numeric> //for accumulate

namespace mdb {
  /*
    Declaration of binary trees
  */
  template<class Leaf>
  class BinaryTree {
    std::variant<Leaf, Indirect<std::pair<BinaryTree, BinaryTree> > > data;
  public:
    explicit BinaryTree(Leaf leaf):data(std::in_place_index<0>, std::move(leaf)) {}
    BinaryTree(BinaryTree lhs, BinaryTree rhs):data(std::in_place_index<1>, mdb::in_place, std::move(lhs), std::move(rhs)) {}

    Leaf* get_if_leaf() { return std::get_if<0>(&data); }
    Leaf const* get_if_leaf() const { return std::get_if<0>(&data); }
    std::pair<BinaryTree, BinaryTree>* get_if_node() {
      if(auto* indirect = std::get_if<1>(&data)) {
        return indirect->get();
      } else {
        return nullptr;
      }
    }
    std::pair<BinaryTree, BinaryTree> const* get_if_node() const {
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
    std::pair<BinaryTree, BinaryTree>& get_node() {
      auto ret = get_if_node();
      assert(ret);
      return *ret;
    }
    std::pair<BinaryTree, BinaryTree> const& get_node() const {
      auto ret = get_if_node();
      assert(ret);
      return *ret;
    }
    template<class F, class G>
    decltype(auto) visit(F&& if_leaf, G&& if_node) {
      if(data.index() == 0) {
        return if_leaf(std::get<0>(data));
      } else {
        auto& [lhs, rhs] = *std::get<1>(data);
        return if_node(lhs, rhs);
      }
    }
    template<class F, class G>
    decltype(auto) visit(F&& if_leaf, G&& if_node) const {
      if(data.index() == 0) {
        return if_leaf(std::get<0>(data));
      } else {
        auto const& [lhs, rhs] = *std::get<1>(data);
        return if_node(lhs, rhs);
      }
    }
  };

  template<class Leaf> Leaf* get_if_leaf(BinaryTree<Leaf>* candidate) { if(candidate) return candidate->get_if_leaf(); else return nullptr; }
  template<class Leaf> Leaf const* get_if_leaf(BinaryTree<Leaf> const* candidate) { if(candidate) return candidate->get_if_leaf(); else return nullptr; }
  template<class Leaf> std::pair<BinaryTree<Leaf>, BinaryTree<Leaf> >* get_if_node(BinaryTree<Leaf>* candidate) { if(candidate) return candidate->get_if_node(); else return nullptr; }
  template<class Leaf> std::pair<BinaryTree<Leaf>, BinaryTree<Leaf> > const* get_if_node(BinaryTree<Leaf> const* candidate) { if(candidate) return candidate->get_if_node(); else return nullptr; }

  template<class Leaf, class F, class G, class T = std::invoke_result_t<F, Leaf const&> >
  T recurse_tree(BinaryTree<Leaf> const& tree, F&& leaf_handler, G&& node_handler) {
    return tree.visit(
      [&](Leaf const& leaf) {
        return leaf_handler(leaf);
      },
      [&](BinaryTree<Leaf> const& lhs, BinaryTree<Leaf> const& rhs) {
        return node_handler(recurse_tree(lhs, leaf_handler, node_handler), recurse_tree(rhs, leaf_handler, node_handler));
      });
  }
  template<class Leaf, class F, class G, class T = std::invoke_result_t<F, Leaf&&> >
  T recurse_tree(BinaryTree<Leaf>&& tree, F&& leaf_handler, G&& node_handler) {
    return tree.visit(
      [&](Leaf& leaf) {
        return leaf_handler(std::move(leaf));
      },
      [&](BinaryTree<Leaf>& lhs, BinaryTree<Leaf>& rhs) {
        return node_handler(recurse_tree(std::move(lhs), leaf_handler, node_handler), recurse_tree(std::move(rhs), leaf_handler, node_handler));
      });
  }
  namespace detail {
    template<class T>
    struct BinaryTreeType;
    template<class T>
    struct BinaryTreeType<BinaryTree<T> > { using Type = T; };
  }
  template<class Leaf, class F, class T = detail::BinaryTreeType<std::invoke_result_t<F, Leaf const&> >::Type > //monad operation = replace each node with a tree
  BinaryTree<T> bind_tree(BinaryTree<Leaf> const& tree, F&& map) {
    return recurse_tree(tree, [&](Leaf const& leaf) -> BinaryTree<T> {
      return map(leaf);
    }, [&](BinaryTree<T> lhs, BinaryTree<T> rhs) {
      return BinaryTree<T>{std::move(lhs), std::move(rhs)};
    });
  }

  template<class Leaf>
  bool operator==(BinaryTree<Leaf> const& me, BinaryTree<Leaf> const& other) {
    return me.visit(
      [&](Leaf const& leaf) {
        if(auto* other_leaf = other.get_if_leaf()) {
          return leaf == *other_leaf;
        } else {
          return false;
        }
      },
      [&](BinaryTree<Leaf> const& lhs, BinaryTree<Leaf> const& rhs) {
        if(auto* other_node = other.get_if_node()) {
          return lhs == other_node->first && rhs == other_node->second;
        } else {
          return false;
        }
      });
  }

  /*
    Declaration of helpers
  */
  enum class TreeStep {
    left,
    right
  };
  struct TreePath {
    std::vector<TreeStep> steps;
  };
  inline TreePath append_path(TreePath root, TreePath const& relative) {
    root.steps.insert(root.steps.end(), relative.steps.begin(), relative.steps.end());
    return root;
  }
  struct UnzipException : std::runtime_error {
    UnzipException():std::runtime_error("Unzipping failed.") {}
  };
  template<class Leaf>
  BinaryTree<Leaf> step_tree_by(BinaryTree<Leaf> tree, TreeStep step) {
    if(auto* pair = tree.get_if_node()) {
      if(step == TreeStep::left) {
        return std::move(pair->first);
      } else {
        return std::move(pair->second);
      }
    } else {
      throw UnzipException{};
    }
  }
  template<class Leaf>
  BinaryTree<Leaf> const& step_tree_by(BinaryTree<Leaf> const& tree, TreeStep step) {
    if(auto* pair = tree.get_if_node()) {
      if(step == TreeStep::left) {
        return pair->first;
      } else {
        return pair->second;
      }
    } else {
      throw UnzipException{};
    }
  }

  template<class Leaf>
  BinaryTree<Leaf> tree_at_path(BinaryTree<Leaf> tree, TreePath const& path) {
    return std::accumulate(path.steps.begin(), path.steps.end(), std::move(tree), [](auto tree, TreeStep step){ return step_tree_by(tree, step); });
  }
  template<class Leaf>
  BinaryTree<Leaf> const& tree_at_path(BinaryTree<Leaf> const& tree, TreePath const& path) {
    return std::accumulate(path.steps.begin(), path.steps.end(), std::move(tree), [](auto const& tree, TreeStep step){ return step_tree_by(tree, step); });
  }

  template<class Leaf>
  struct Zipper {
    BinaryTree<Leaf> head;
    std::vector<std::pair<TreeStep, BinaryTree<Leaf> > > zips;
    BinaryTree<Leaf> zip() && {
      for(auto it = zips.rbegin(); it != zips.rend(); ++it) {
        auto& [step, pair] = *it;
        if(step == TreeStep::left) {
          head = BinaryTree<Leaf>{std::move(head), std::move(pair)};
        } else {
          head = BinaryTree<Leaf>{std::move(pair), std::move(head)};
        }
      }
      return head;
    }
    BinaryTree<Leaf> zip() const& {
      for(auto it = zips.rbegin(); it != zips.rend(); ++it) {
        auto& [step, pair] = *it;
        if(step == TreeStep::left) {
          head = BinaryTree<Leaf>{head, pair};
        } else {
          head = BinaryTree<Leaf>{pair, head};
        }
      }
      return head;
    }
    void step(TreeStep step) {
      if(auto* pair = head.get_if_node()) {
        auto [lhs, rhs] = std::move(*pair); //have to move these before head is destructed
        if(step == TreeStep::left) {
          head = std::move(lhs);
          zips.emplace_back(TreeStep::left, std::move(rhs));
        } else {
          head = std::move(rhs);
          zips.emplace_back(TreeStep::right, std::move(lhs));
        }
      } else {
        throw UnzipException{};
      }
    }
  };
  template<class Leaf>
  Zipper<Leaf> zipper_from_tree(BinaryTree<Leaf> tree) {
    return Zipper{.head = std::move(tree)};
  }
  template<class Leaf>
  Zipper<Leaf> unzip_tree(BinaryTree<Leaf> tree, TreePath const& path) {
    auto ret = zipper_from_tree(std::move(tree));
    for(auto const& element : path.steps) {
      ret.step(element);
    }
    return ret;
  }
  template<class Leaf>
  BinaryTree<Leaf> replace_term_at(BinaryTree<Leaf> target, TreePath const& path, BinaryTree<Leaf> replacement) {
    auto zip = unzip_tree(std::move(target), path);
    zip.head = std::move(replacement);
    return std::move(zip).zip();
  }

  /*
    Iterators
  */
  struct RecursiveTreeSentinel {};
  template<class Leaf>
  struct RecursiveTreeConstIterator {
    TreePath path;
    std::vector<BinaryTree<Leaf> const*> stack;
    std::pair<TreePath const&, BinaryTree<Leaf> const&> operator*() const {
      return {path, *stack.back()};
    }
    void operator++() {
      if(auto* pair = stack.back()->get_if_node()) {
        path.steps.push_back(TreeStep::left);
        stack.push_back(&pair->first);
      } else {
        while(!path.steps.empty() && path.steps.back() == TreeStep::right) {
          path.steps.pop_back();
          stack.pop_back();
        }
        if(path.steps.empty()) {
          //didn't find anywhere to go right.
          stack.pop_back();
        } else {
          //descend one step further (past the left) then go right
          path.steps.pop_back();
          stack.pop_back();

          path.steps.push_back(TreeStep::right);
          stack.push_back(&stack.back()->get_node().second);
        }
      }
    }
    bool operator!=(RecursiveTreeSentinel) const {
      return !stack.empty();
    }
  };
  template<class Leaf>
  struct RecursiveTreeConstRange {
    BinaryTree<Leaf> const& target;
    auto begin() const { return RecursiveTreeConstIterator<Leaf>{.stack = {&target}}; }
    auto end() const { return RecursiveTreeSentinel{}; }
  };
  template<class Leaf>
  auto recursive_range_of(BinaryTree<Leaf> const& target) {
    return RecursiveTreeConstRange<Leaf>{target};
  }
}

#endif
