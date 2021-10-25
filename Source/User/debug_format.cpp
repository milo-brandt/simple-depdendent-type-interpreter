#include "debug_format.hpp"
#include "../Utility/overloaded.hpp"

namespace user {
  RawFormat raw_format(new_expression::Arena& arena, new_expression::WeakExpression expression) {
    return raw_format(arena, expression, [&arena](std::ostream& o, new_expression::WeakExpression expr) {
      if(arena.holds_declaration(expr)) {
        o << "decl_" << expr.data();
      } else if(arena.holds_axiom(expr)) {
        o << "ax_" << expr.data();
      } else {
        o << "???";
      }
    });
  }
  RawFormat raw_format(new_expression::Arena& arena, new_expression::WeakExpression expression, mdb::function<void(std::ostream&, new_expression::WeakExpression)> format_external) {
    return RawFormat{
      .arena = arena,
      .expression = expression,
      .format_external = std::move(format_external)
    };
  }
  std::ostream& operator<<(std::ostream& o, RawFormat const& raw) {
    struct Detail {
      new_expression::Arena& arena;
      mdb::function<void(std::ostream&, new_expression::WeakExpression)> const& format_external;
      std::ostream& o;
      void write(new_expression::WeakExpression expression, bool parenthesize_application) {
        arena.visit(expression, mdb::overloaded{
          [&](new_expression::Apply const& apply) {
            if(parenthesize_application) o << "(";
            write(apply.lhs, false);
            o << " ";
            write(apply.rhs, true);
            if(parenthesize_application) o << ")";
          },
          [&](new_expression::Argument const& arg) {
            o << "$" << arg.index;
          },
          [&](new_expression::Axiom const&) {
            format_external(o, expression);
          },
          [&](new_expression::Declaration const&) {
            format_external(o, expression);
          },
          [&](new_expression::Data const& data) {
            arena.debug_print_data(o, data);
          },
          [&](new_expression::Conglomerate const& conglomerate) {
            o << "#" << conglomerate.index;
          }
        });
      }
    };
    Detail{raw.arena, raw.format_external, o}.write(raw.expression, false);
    return o;
  }
}
