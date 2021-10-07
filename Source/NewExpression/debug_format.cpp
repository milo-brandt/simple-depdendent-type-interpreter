#include "debug_format.hpp"
#include "../Utility/overloaded.hpp"

namespace new_expression {
  RawFormat raw_format(Arena const& arena, WeakExpression expr) {
    return RawFormat{
      arena,
      expr
    };
  }
  std::ostream& operator<<(std::ostream& o, RawFormat const& raw) {
    struct Detail {
      Arena const& arena;
      std::ostream& o;
      void write(WeakExpression expression, bool parenthesize_application) {
        arena.visit(expression, mdb::overloaded{
          [&](Apply const& apply) {
            if(parenthesize_application) o << "(";
            write(apply.lhs, false);
            o << " ";
            write(apply.rhs, true);
            if(parenthesize_application) o << ")";
          },
          [&](Argument const& arg) {
            o << "$" << arg.index;
          },
          [&](Axiom const& external) {
            o << "ax_" << expression.index();
          },
          [&](Declaration const& declaration) {
            o << "dec_" << expression.index();
          },
          [&](Data const& data) {
            o << "<data>";
          },
          [&](Conglomerate const& conglomerate) {
            o << "#" << conglomerate.index;
          }
        });
      }
    };
    Detail{raw.arena, o}.write(raw.expression, false);
    return o;
  }
}
