#ifndef EXPRESSION_FORMATTER_HPP
#define EXPRESSION_FORMATTER_HPP

#include "expression_tree.hpp"
#include "evaluation_context.hpp"
#include <unordered_set>
#include <string>
#include <functional>
#include <sstream>
/*
  Future work - add explicit support for printing lambdas by
  recognizing lambdas in context and adding new terms to
  represent them (allowing for their rule to be simplified
  in an interactive manner)
*/

namespace expression::format {
  struct FormatContext;
  struct Formatter {
    FormatContext& context;
    tree::Expression const& expr;
  };
  struct FormatContext {
    Context& expression_context;
    std::function<bool(std::uint64_t)> force_expansion; //for lambdas
    std::function<void(std::ostream&, std::uint64_t)> write_external;
    Formatter operator()(tree::Expression const& expr) & {
      return Formatter{*this, expr};
    }
    Formatter operator()(tree::Expression const& expr) && {
      return (*this)(expr);
    }
  };
  std::ostream& operator<<(std::ostream&, Formatter);
}


#endif
