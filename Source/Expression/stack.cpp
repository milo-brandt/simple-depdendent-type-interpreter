#include "stack.hpp"

namespace expression {
  struct Stack::Impl {
    struct ParentInfo {
      std::shared_ptr<Impl> parent;
      tree::Expression extension_family;
    };
    std::optional<ParentInfo> parent;
    std::uint64_t depth;
    tree::Expression fam;
    tree::Expression var;
    Impl(std::optional<ParentInfo> parent, std::uint64_t depth, tree::Expression fam, tree::Expression var):parent(std::move(parent)),depth(depth),fam(std::move(fam)),var(std::move(var)){}
  };
  Stack::Stack(std::shared_ptr<Impl> impl):impl(std::move(impl)) {}

  std::uint64_t Stack::depth() const { return impl->depth; }
  tree::Expression Stack::type_family_type() const { return impl->fam; }
  tree::Expression Stack::apply_args(tree::Expression in) const {
    for(std::uint64_t i = 0; i < depth(); ++i) {
      in = tree::Apply{
        std::move(in),
        tree::Arg{i}
      };
    }
    return std::move(in);
  }
  tree::Expression Stack::instance_of_type_family(Context& context, tree::Expression expr) const {
    auto var = context.create_variable({
      .is_axiom = false,
      .type = type_family_type()
    });
    context.rules.push_back({
      .pattern = expression::lambda_pattern(var, depth()),
      .replacement = std::move(expr)
    });
    return tree::Apply{
      impl->var,
      tree::External{var}
    };
  }
  tree::Expression Stack::type_of_arg(std::uint64_t i) const {
    if(i >= impl->depth) std::terminate();
    Impl* ptr = impl.get();
    while(i != ptr->depth - 1) {
      if(!ptr->parent || !ptr->parent->parent) std::terminate();
      ptr = ptr->parent->parent.get();
    }
    if(!ptr->parent) std::terminate();
    return ptr->parent->extension_family;
  }

  Stack Stack::empty(Context& context) {
    return Stack{std::make_shared<Impl>(
      std::nullopt,
      0,
      tree::External{context.primitives.type}, //Type
      tree::Apply{ //\x:Type.x
        tree::External{context.primitives.id},
        tree::External{context.primitives.type}
      }
    )};
  }
  Stack Stack::extend(Context& context, tree::Expression extension_family) const {
    return context.create_variables<4>([&](auto&& build, auto inner_constant_family, auto family_over, auto as_fibration, auto var_p) {
      tree::Expression new_fam = tree::Apply{
        impl->var,
        tree::External{family_over}
      };
      build(expression::ExternalInfo{ //inner_constant_family
        .is_axiom = false,
        .type = new_fam
      }, expression::ExternalInfo{ //family_over
        .is_axiom = false,
        .type = impl->fam
      }, expression::ExternalInfo{ //as_fibration
        .is_axiom = false,
        .type = expression::multi_apply(
          tree::External{context.primitives.arrow},
          new_fam,
          expression::multi_apply(
            tree::External{context.primitives.constant},
            tree::External{context.primitives.type},
            tree::External{context.primitives.type},
            new_fam
          )
        )
      }, expression::ExternalInfo{ //var_p
        .is_axiom = false,
        .type = tree::Apply{
          tree::External{context.primitives.type_family},
          new_fam
        }
      });
      auto apply_args = [](tree::Expression head, std::uint64_t arg_start, std::uint64_t arg_count) {
        for(std::uint64_t i = 0; i < arg_count; ++i) {
          head = tree::Apply{std::move(head), tree::Arg{arg_start + i}};
        }
        return head;
      };
      context.rules.push_back({
        .pattern = expression::lambda_pattern(inner_constant_family, impl->depth + 1),
        .replacement = tree::External{context.primitives.type}
      });
      context.rules.push_back({
        .pattern = expression::lambda_pattern(family_over, impl->depth),
        .replacement = expression::multi_apply(
          tree::External{context.primitives.arrow},
          apply_args(extension_family, 0, impl->depth),
          apply_args(tree::External{inner_constant_family}, 0, impl->depth)
        )
      });
      context.rules.push_back({
        .pattern = expression::lambda_pattern(as_fibration, impl->depth + 1),
        .replacement = expression::multi_apply(
          tree::External{context.primitives.arrow},
          apply_args(extension_family, 1, impl->depth),
          apply_args(tree::Arg{0}, 1, impl->depth)
        )
      });
      context.rules.push_back({
        .pattern = expression::lambda_pattern(var_p, 1),
        .replacement = tree::Apply{
          impl->var,
          tree::Apply{
            tree::External{as_fibration},
            tree::Arg{0}
          }
        }
      });
      return Stack{std::make_shared<Impl>(
        Impl::ParentInfo{
          .parent = impl,
          .extension_family = std::move(extension_family),
        },
        impl->depth + 1,
        std::move(new_fam),
        expression::tree::External{var_p}
      )};
    });
  }
  tree::Expression Stack::type_of(Context& context, tree::Expression expression) const {
    return expression.visit(mdb::overloaded{
      [&](tree::Apply& apply) -> tree::Expression {
        auto lhs_type = type_of(context, apply.lhs);
        if(auto func_data = context.get_domain_and_codomain(std::move(lhs_type))) {
          return tree::Apply{std::move(func_data->codomain), std::move(apply.rhs)};
        } else {
          std::terminate();
        }
      },
      [&](tree::External& external) -> tree::Expression {
        return context.external_info.at(external.external_index).type;
      },
      [&](tree::Arg& arg) -> tree::Expression {
        return type_of_arg(arg.arg_index);
      },
      [&](tree::Data& data) -> tree::Expression {
        return data.data.type_of();
      }
    });
  }
}
