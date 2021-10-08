#include "arena_utility.hpp"
#include "arena_container.hpp"
#include "../Utility/overloaded.hpp"

namespace new_expression {
  Unfolding unfold(Arena& arena, WeakExpression expr) {
    Unfolding ret{
      .head = expr
    };
    while(auto* apply = arena.get_if_apply(ret.head)) {
      ret.head = apply->lhs;
      ret.args.push_back(apply->rhs);
    }
    std::reverse(ret.args.begin(), ret.args.end());
    return ret;
  }
  OwnedUnfolding unfold_owned(Arena& arena, OwnedExpression expr) {
    WeakExpression head = expr;
    std::vector<OwnedExpression> args;
    while(auto* apply = arena.get_if_apply(head)) {
      head = apply->lhs;
      args.push_back(arena.copy(apply->rhs));
    }
    std::reverse(args.begin(), args.end());
    OwnedUnfolding ret {
      .head = arena.copy(head),
      .args = std::move(args)
    };
    arena.drop(std::move(expr));
    return ret;
  }
  namespace {
    template<class ArgType>
    struct Detail {
      Arena& arena;
      std::span<ArgType const> args;
      std::unordered_map<WeakExpression, WeakExpression, ExpressionHasher> memoized_results;
      OwnedExpression substitute(WeakExpression target) {
        if(memoized_results.contains(target)) return arena.copy(memoized_results.at(target));
        auto ret = arena.visit(target, mdb::overloaded{
          [&](Apply const& apply) {
            return arena.apply(substitute(apply.lhs), substitute(apply.rhs));
          },
          [&](Argument const& arg) {
            if(args.size() <= arg.index) std::terminate(); //not enough args.
            return arena.copy(args[arg.index]);
          },
          [&](Axiom const& axiom) {
            return arena.copy(target);
          },
          [&](Declaration const& declaration) {
            return arena.copy(target);
          },
          [&](auto const&) -> OwnedExpression {
            std::terminate();
          }
        });
        memoized_results.insert(std::make_pair(target, (WeakExpression)ret));
        return ret;
      }
    };
  }
  OwnedExpression substitute_into(Arena& arena, WeakExpression target, std::span<OwnedExpression const> args) {
    return Detail<OwnedExpression>{arena, args}.substitute(target);
  }
  OwnedExpression substitute_into(Arena& arena, WeakExpression target, std::span<WeakExpression const> args) {
    return Detail<WeakExpression>{arena, args}.substitute(target);
  }
}
