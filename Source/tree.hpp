#ifndef TREE_HPP
#define TREE_HPP

#include <variant>
#include <memory>
#include <concepts>
#include <functional>
#include "indirect_value.hpp"

template<class leaf_type, class label_type>
class binary_tree {
  std::variant<leaf_type, std::tuple<label_type, indirect_value<binary_tree>, indirect_value<binary_tree> > > data;
public:
  binary_tree(leaf_type leaf):data(std::in_place_index<0>, std::move(leaf)){}
  binary_tree(label_type label, binary_tree lhs, binary_tree rhs):data(std::in_place_index<1>, std::move(label), std::move(lhs), std::move(rhs)){}
  bool is_leaf() const{ return data.index() == 0; }
  leaf_type& as_leaf() &{ return std::get<0>(data); }
  leaf_type& as_leaf() &&{ return std::get<0>(std::move(data)); }
  leaf_type& as_leaf() const&{ return std::get<0>(data); }
  leaf_type& as_leaf() const&&{ return std::get<0>(std::move(data)); }
  std::tuple<label_type&, binary_tree&, binary_tree&> as_node() &{ auto v = std::get<1>(data); return {std::get<0>(v), std::get<1>(v), std::get<2>(v)}; }
  std::tuple<label_type&&, binary_tree&&, binary_tree&&> as_node() &&{ auto v = std::get<1>(data); return {std::move(std::get<0>(v)), std::move(std::get<1>(v)), std::move(std::get<2>(v))}; }
  std::tuple<const label_type&, const binary_tree&, const binary_tree&> as_node() const&{ auto v = std::get<1>(data); return {std::get<0>(v), std::get<1>(v), std::get<2>(v)}; }
  std::tuple<const label_type&&, const binary_tree&&, const binary_tree&&> as_node() const&&{ auto v = std::get<1>(data); return {std::move(std::get<0>(v)), std::move(std::get<1>(v)), std::move(std::get<2>(v))}; }
  template<std::invocable<leaf_type&> leaf_call, std::invocable<label_type&, binary_tree&, binary_tree&> node_call, class ret_type = std::common_type_t<std::invoke_result_t<leaf_call, leaf_type&>, std::invoke_result_t<node_call, label_type&, binary_tree&, binary_tree&> > >
  ret_type visit(leaf_call&& leaf, node_call&& node) &{
    if(is_leaf()) return std::invoke(leaf, as_leaf());
    else return std::apply(node, as_node());
  }
  template<std::invocable<leaf_type&&> leaf_call, std::invocable<label_type&&, binary_tree&&, binary_tree&&> node_call, class ret_type = std::common_type_t<std::invoke_result_t<leaf_call, leaf_type&&>, std::invoke_result_t<node_call, label_type&&, binary_tree&&, binary_tree&&> > >
  ret_type visit(leaf_call&& leaf, node_call&& node) &&{
    if(is_leaf()) return std::invoke(leaf, std::move(*this).as_leaf());
    else return std::apply(node, std::move(*this).as_node());
  }
  template<std::invocable<leaf_type const&> leaf_call, std::invocable<label_type const&, binary_tree const&, binary_tree const&> node_call, class ret_type = std::common_type_t<std::invoke_result_t<leaf_call, leaf_type const&>, std::invoke_result_t<node_call, label_type const&, binary_tree const&, binary_tree const&> > >
  ret_type visit(leaf_call&& leaf, node_call&& node) const& {
    if(is_leaf()) return std::invoke(leaf, as_leaf());
    else return std::apply(node, as_node());
  }
  template<std::invocable<leaf_type const&&> leaf_call, std::invocable<label_type const&&, binary_tree const&&, binary_tree const&&> node_call, class ret_type = std::common_type_t<std::invoke_result_t<leaf_call, leaf_type const&&>, std::invoke_result_t<node_call, label_type const&&, binary_tree const&&, binary_tree const&&> > >
  ret_type visit(leaf_call&& leaf, node_call&& node) const&& {
    if(is_leaf()) return std::invoke(leaf, std::move(*this).as_leaf());
    else return std::apply(node, std::move(*this).as_node());
  }

};

#endif
