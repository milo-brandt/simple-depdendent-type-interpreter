#ifndef OPERATOR_TREE_HPP
#define OPERATOR_TREE_HPP

#include <variant>
#include <memory>
#include <string>
#include <iostream>
#include "template_utility.hpp"

namespace operator_tree {
  template<class binop_t, class unop_t>
  struct binary_operation;
  template<class binop_t, class unop_t>
  struct unary_operation;
  struct ident {
    std::string_view id;
  };
  template<class binop_t, class unop_t>
  using tree_data = std::variant<
    binary_operation<binop_t, unop_t>,
    unary_operation<binop_t, unop_t>,
    ident
  >;
  template<class binop_t, class unop_t>
  using tree = std::shared_ptr<tree_data<binop_t, unop_t> >;
  template<class binop_t, class unop_t>
  struct binary_operation {
    binop_t op;
    std::string_view span;
    std::string_view op_span;
    tree<binop_t, unop_t> lhs;
    tree<binop_t, unop_t> rhs;
  };
  template<class binop_t, class unop_t>
  struct unary_operation {
    unop_t op;
    std::string_view span;
    std::string_view op_span;
    tree<binop_t, unop_t> operand;
  };
  template<class binop_t, class unop_t>
  tree<binop_t, unop_t> join_binary(binop_t op, std::string_view span, std::string_view op_span, tree<binop_t, unop_t> lhs, tree<binop_t, unop_t> rhs) {
    return std::make_shared<tree_data<binop_t, unop_t> >(binary_operation<binop_t, unop_t>{std::move(op), span, op_span, std::move(lhs), std::move(rhs)});
  }
  template<class binop_t, class unop_t>
  tree<binop_t, unop_t> join_unary(unop_t op, std::string_view span, std::string_view op_span, tree<binop_t, unop_t> operand) {
    return std::make_shared<tree_data<binop_t, unop_t> >(unary_operation<binop_t, unop_t>{std::move(op), span, op_span, std::move(operand)});
  }
  template<class binop_t, class unop_t>
  tree<binop_t, unop_t> make_identifier(std::string_view id) {
    return std::make_shared<tree_data<binop_t, unop_t> >(ident{id});
  }
  template<class binop_t, class unop_t>
  std::string_view get_span_of(tree<binop_t, unop_t> const& tree) {
    if(auto* ptr = std::get_if<0>(tree.get())) return ptr->span;
    if(auto* ptr = std::get_if<1>(tree.get())) return ptr->span;
    if(auto* ptr = std::get_if<2>(tree.get())) return ptr->id;
    std::terminate();
  }
  template<class binop_t, class unop_t>
  std::ostream& operator<<(std::ostream& o, tree<binop_t, unop_t> const& tree) {
    if(!tree) return o << "(null tree)";
    if(auto* ptr = std::get_if<0>(tree.get())) return o << "(" << ptr->lhs << " " << ptr->op_span << " " << ptr->rhs << ")";
    if(auto* ptr = std::get_if<1>(tree.get())) return o << "(" << ptr->op_span << " " << ptr->operand << ")";
    if(auto* ptr = std::get_if<2>(tree.get())) return o << ptr->id;
    std::terminate();
  }
}

#endif
