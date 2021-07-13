#ifndef CRTP_TREE_HPP
#define CRTP_TREE_HPP

#include "indirect.hpp"
#include <variant>
#include <tuple>

namespace mdb {

  template<template<template<class> class> class Data, template<class> class... Shapes>
  struct Tree {
    public:
      template<template<class> class _Shape>
      struct Entry : _Shape<Tree>, Data<_Shape> {
        template<class Tree>
        using Shape = _Shape<Tree>;
        using ShapeKind = Shape<Tree>;
        Shape<Tree>& get_shape() { return (Shape<Tree>&)(*this); }
        Data<Shape>& get_data() { return (Data<Shape>&)(*this); }
        Shape<Tree> const& get_shape() const { return (Shape<Tree>&)(*this); }
        Data<Shape> const& get_data() const { return (Data<Shape>&)(*this); }
      };
    //private:
      Indirect<std::variant<Entry<Shapes>... > > data;
    public:
      template<template<class> class Shape>
      Tree(Entry<Shape> entry):data(in_place, std::move(entry)) {}
  };

  namespace example_shape {
    template<class Tree>
    struct Apply {
      Tree lhs;
      Tree rhs;
      static constexpr auto __members = std::make_tuple(&Apply::lhs, &Apply::rhs);
    };
    template<class Tree>
    struct Lambda {
      Tree body;
      static constexpr auto __members = std::make_tuple(&Lambda::body);
    };
    template<class Tree>
    struct Identifier {
      static constexpr auto __members = std::make_tuple();
    };


    template<template<template<class> class> class Data>
    using Expression = mdb::Tree<Data, Apply, Lambda, Identifier>;

  }
  namespace example {
    template<template<class> class> struct Data;
    template<> struct Data<example_shape::Apply> { int how_bad; };
    template<> struct Data<example_shape::Lambda> { int hoo_boy; };
    template<> struct Data<example_shape::Identifier> { int how_boy; };

    using Tree = example_shape::Expression<Data>;

    using Apply = Tree::Entry<example_shape::Apply>;
    using Lambda = Tree::Entry<example_shape::Lambda>;
    using Identifier = Tree::Entry<example_shape::Identifier>;
  }

  template<class Ret, class Visitor, template<template<class> class> class Data, template<class> class Shape>
  concept VisitableRValue = requires(Visitor visitor, Shape<Ret> shape, Data<Shape> data) {
    visitor(std::move(shape), std::move(data));
  };
  template<class Ret, class Visitor, template<template<class> class> class Data, template<class> class... Shapes>
  Ret recurse_tree(mdb::Tree<Data, Shapes...>&& tree, Visitor&& visitor) requires (VisitableRValue<Ret, Visitor, Data, Shapes> && ...) {
    return std::visit(
      [&]<class T>(T&& node) {
        return std::apply([&](auto const&... members) {
          return visitor(
            typename T::template Shape<Ret>{recurse_tree<Ret>(std::move(node.*members), visitor)...},
            node.get_data()
          );
        }, T::ShapeKind::__members);
      },
      std::move(*tree.data)
    );
  }
  template<class Ret, class Visitor, template<template<class> class> class... Data, template<class> class... Shapes>
  Ret recurse_tree_tuple(std::tuple<mdb::Tree<Data, Shapes...>...> const& tree, Visitor&& visitor) {

  }


  template<class Ret, class Visitor, template<template<class> class> class Data, template<class> class Shape>
  concept VisitableLValue = requires(Visitor visitor, Shape<Ret> const& shape, Data<Shape> const& data) {
    visitor(shape, data);
  };
  template<class Ret, class Visitor, template<template<class> class> class Data, template<class> class... Shapes>
  Ret recurse_tree(mdb::Tree<Data, Shapes...> const& tree, Visitor&& visitor) requires (VisitableLValue<Ret, Visitor, Data, Shapes> && ...) {
    return std::visit(
      [&]<class T>(T&& node) {
        return std::apply([&](auto const&... members) {
          return visitor(
            typename T::template Shape<Ret>{recurse_tree<Ret>(node.*members, visitor)...},
            node.get_data()
          );
        }, T::ShapeKind::__members);
      },
      std::move(*tree.data)
    );
  }

}

#endif
