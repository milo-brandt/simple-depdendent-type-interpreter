#include "expression_debug_format.hpp"

namespace expression {
  RawFormat raw_format(tree::Expression const& expr) {
    return raw_format(expr, [](std::ostream& o, std::uint64_t index) {
      o << "ext_" << index;
    });
  }
  RawFormat raw_format(tree::Expression const& expr, mdb::function<void(std::ostream&, std::uint64_t)> format_external) {
    return RawFormat{expr, std::move(format_external)};
  }
  std::ostream& operator<<(std::ostream& o, RawFormat const& raw) {
    struct Detail {
      mdb::function<void(std::ostream&, std::uint64_t)> const& format_external;
      std::ostream& o;
      void write(tree::Expression const& expression, bool parenthesize_application) {
        expression.visit(mdb::overloaded{
          [&](tree::Apply const& apply) {
            if(parenthesize_application) o << "(";
            write(apply.lhs, false);
            o << " ";
            write(apply.rhs, true);
            if(parenthesize_application) o << ")";
          },
          [&](tree::Arg const& arg) {
            o << "$" << arg.arg_index;
          },
          [&](tree::External const& external) {
            format_external(o, external.external_index);
          },
          [&](tree::Data const& data) {
            o << data.data;
          }
        });
      }
    };
    Detail{raw.format_external, o}.write(raw.expression, false);
    return o;
  }
}
