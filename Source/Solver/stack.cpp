#include "stack.hpp"
#include "../Utility/overloaded.hpp"
#include "../NewExpression/arena_utility.hpp"
bool is_free_from_conglomerates(new_expression::Arena& arena, new_expression::WeakExpression expr) {
  return arena.visit(expr, mdb::overloaded{
    [&](new_expression::Apply const& apply) {
      return is_free_from_conglomerates(arena, apply.lhs)
          && is_free_from_conglomerates(arena, apply.rhs);
    },
    [&](new_expression::Conglomerate const&) {
       return false;
    },
    [&](auto const&) {
      return true;
    }
  });
}

namespace stack {
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using TypedValue = new_expression::TypedValue;
  using Arena = new_expression::Arena;
  using EvaluationContext = new_expression::EvaluationContext;
  struct Stack::Impl {
    struct ParentInfo {
      std::shared_ptr<Impl> parent;
      std::optional<OwnedExpression> extension_family; //left blank for assumptions
    };
    StackInterface interface;
    std::optional<ParentInfo> parent;
    std::uint64_t depth;
    OwnedExpression fam;
    OwnedExpression var;
    std::shared_ptr<EvaluationContext> evaluation;
    Impl(StackInterface interface, std::optional<ParentInfo> parent, std::uint64_t depth, OwnedExpression fam, OwnedExpression var, std::shared_ptr<EvaluationContext> evaluation):interface(interface), parent(std::move(parent)),depth(depth),fam(std::move(fam)),var(std::move(var)),evaluation(std::move(evaluation)) {}
    ~Impl() {
      if(parent && parent->extension_family) {
        interface.arena.drop(std::move(*parent->extension_family));
      }
      destroy_from_arena(interface.arena, fam, var);
    }
    /*
    fam : Type = ($0 : ctx_0) -> ($1 : ctx_1 $0) -> ($2 : ctx_2 $0 $1) -> ... -> Type
    var : fam -> Type = (F : fam) -> \$0:ctx_0.\$1:ctx_1 $0.\$2:ctx_2 $0 $1. ... .F $0 $1 $2 ... $n
    */
  };
  Stack::Stack(std::shared_ptr<Impl> impl):impl(std::move(impl)) {}

  std::uint64_t Stack::depth() const { return impl->depth; }
  WeakExpression Stack::type_family_type() const { return impl->fam; }
  OwnedExpression Stack::apply_args(OwnedExpression in) const {
    for(std::uint64_t i = 0; i < depth(); ++i) {
      in = impl->interface.arena.apply(
        std::move(in),
        impl->interface.arena.argument(i)
      );
    }
    return std::move(in);
  }
  OwnedExpression Stack::instance_of_type_family(OwnedExpression expr) const {
    auto v = impl->interface.arena.declaration();
    impl->interface.register_type(v, impl->interface.arena.copy(impl->fam));
    impl->interface.register_declaration(v);
    impl->interface.add_rule({
      .pattern = lambda_pattern(impl->interface.arena.copy(v), depth()),
      .replacement = eliminate_conglomerates(std::move(expr))
    });
    return impl->interface.arena.apply(
      impl->interface.arena.copy(impl->var),
      std::move(v)
    );
  }
  OwnedExpression Stack::type_of_arg(std::uint64_t i) const {
    if(i >= impl->depth) std::terminate();
    Impl* ptr = impl.get();
    while(i != ptr->depth - 1 || !ptr->parent->extension_family) {
      if(!ptr->parent || !ptr->parent->parent) std::terminate();
      ptr = ptr->parent->parent.get();
    }
    if(!ptr->parent) std::terminate();
    return ptr->interface.arena.copy(*ptr->parent->extension_family);
  }
  OwnedExpression Stack::type_of(WeakExpression expr) const {
    return new_expression::get_type_of(impl->interface.arena, expr, {
      .arrow = impl->interface.arrow,
      .type_of_primitive = [&](WeakExpression expr) {
        return impl->interface.arena.visit(expr, mdb::overloaded{
          [&](new_expression::Apply const&) -> OwnedExpression { std::terminate(); }, //not primitive
          [&](new_expression::Argument const& arg) {
            return type_of_arg(arg.index);
          },
          [&](new_expression::Conglomerate const& conglom) {
            auto equiv = impl->evaluation->eliminate_conglomerates(impl->interface.arena.copy(expr));
            auto ret = type_of(equiv);
            impl->interface.arena.drop(std::move(equiv));
            return ret;
          },
          [&](auto const&) {
            return impl->interface.arena.copy(
              impl->interface.type_collector.type_of_primitive.at(expr)
            );
          }
        });
      },
      .reduce_head = [&](OwnedExpression expr) {
        return impl->evaluation->reduce(std::move(expr));
      }
    });
  }
  Stack Stack::empty(StackInterface context) {
    auto& arena = context.arena;
    auto type = arena.copy(context.type);
    auto id_type = arena.apply(
      arena.copy(context.id),
      arena.copy(context.type)
    );
    return Stack{std::make_shared<Impl>(
      std::move(context),
      std::nullopt,
      0,
      std::move(type), //Type
      std::move(id_type), //\T:Type.T
      std::make_shared<EvaluationContext>(arena, context.rule_collector)
    )};
  }
  Stack Stack::extend(OwnedExpression extension_family) const {
    auto& arena = impl->interface.arena;
    auto extension_ext = arena.declaration();
    impl->interface.register_type(
      extension_ext,
      arena.copy(impl->fam)
    );
    impl->interface.register_declaration(extension_ext);
    impl->interface.add_rule({
      .pattern = lambda_pattern(arena.copy(extension_ext), impl->depth),
      .replacement = eliminate_conglomerates(arena.copy(extension_family))
    });

    auto inner_constant_family = arena.declaration();
    impl->interface.register_declaration(inner_constant_family);
    auto family_over = arena.declaration();
    impl->interface.register_declaration(family_over);
    auto as_fibration = arena.declaration();
    impl->interface.register_declaration(as_fibration);
    auto var_p = arena.declaration();
    impl->interface.register_declaration(var_p);


    auto new_fam = arena.apply(
      arena.copy(impl->var),
      arena.copy(family_over)
    );

    impl->interface.register_type(
      inner_constant_family,
      arena.copy(new_fam)
    );
    impl->interface.register_type(
      family_over,
      arena.copy(impl->fam)
    );
    impl->interface.register_type(
      as_fibration,
      arena.apply(
        arena.copy(impl->interface.arrow),
        arena.copy(new_fam),
        arena.apply(
          arena.copy(impl->interface.constant),
          arena.copy(impl->interface.type),
          arena.copy(impl->fam),
          arena.copy(new_fam)
        )
      )
    );
    impl->interface.register_type(
      var_p,
      arena.apply(
        arena.copy(impl->interface.type_family),
        arena.copy(new_fam)
      )
    );
    auto apply_args = [&](OwnedExpression head, std::uint64_t arg_start, std::uint64_t arg_count) {
      for(std::uint64_t i = 0; i < arg_count; ++i) {
        head = arena.apply(std::move(head), arena.argument(arg_start + i));
      }
      return head;
    };
    impl->interface.add_rule({
      .pattern = lambda_pattern(arena.copy(inner_constant_family), impl->depth + 1),
      .replacement = eliminate_conglomerates(arena.copy(impl->interface.type))
    });
    impl->interface.add_rule({
      .pattern = lambda_pattern(arena.copy(family_over), impl->depth),
      .replacement = eliminate_conglomerates(arena.apply(
        arena.copy(impl->interface.arrow),
        arena.copy(extension_family),
        apply_args(arena.copy(inner_constant_family), 0, impl->depth)
      ))
    });
    impl->interface.add_rule({
      .pattern = lambda_pattern(arena.copy(as_fibration), impl->depth + 1),
      .replacement = eliminate_conglomerates(arena.apply(
        arena.copy(impl->interface.arrow),
        apply_args(arena.copy(extension_ext), 1, impl->depth),
        apply_args(arena.argument(0), 1, impl->depth)
      ))
    });
    impl->interface.add_rule({
      .pattern = lambda_pattern(arena.copy(var_p), 1),
      .replacement = eliminate_conglomerates(arena.apply(
        arena.copy(impl->var),
        arena.apply(
          arena.copy(as_fibration),
          arena.argument(0)
        )
      ))
    });
    destroy_from_arena(arena,
      extension_ext, inner_constant_family, family_over, as_fibration
    );
    return Stack{std::make_shared<Impl>(
      impl->interface,
      Impl::ParentInfo{
        .parent = impl,
        .extension_family = std::move(extension_family),
      },
      impl->depth + 1,
      std::move(new_fam),
      std::move(var_p),
      impl->evaluation
    )};
  }
  Stack Stack::extend_by_assumption(TypedValue lhs, TypedValue rhs) const {
    auto new_evaluation = std::make_shared<EvaluationContext>(*impl->evaluation);
    new_evaluation->assume_equal(std::move(lhs.type), std::move(rhs.type));
    new_evaluation->assume_equal(std::move(lhs.value), std::move(rhs.value));
    return Stack{std::make_shared<Impl>(
      impl->interface,
      Impl::ParentInfo{
        .parent = impl,
        .extension_family = std::nullopt
      },
      impl->depth,
      impl->interface.arena.copy(impl->fam),
      impl->interface.arena.copy(impl->var),
      std::move(new_evaluation)
    )};
  }
  OwnedExpression Stack::reduce(OwnedExpression expr) const {
    if(auto err = impl->evaluation->canonicalize_context()) {
      return std::move(expr); //er
    } else {
      return impl->evaluation->reduce(std::move(expr));
    }
  }
  OwnedExpression Stack::eliminate_conglomerates(new_expression::OwnedExpression expr) const {
    if(auto err = impl->evaluation->canonicalize_context()) {
      return std::move(expr); //er
    } else {
      auto ret = impl->evaluation->eliminate_conglomerates(std::move(expr));
      if(!is_free_from_conglomerates(impl->interface.arena, ret)) std::terminate();
      return ret;
    }
  }
  new_expression::AssumptionInfo Stack::list_assumptions() const {
    return impl->evaluation->list_assumptions();
  }
  std::optional<new_expression::PartialMap> Stack::try_to_map_to(Stack& target, new_expression::MapRequest request) const {
    return impl->evaluation->try_to_map_to(*target.impl->evaluation, std::move(request));
  }
}
