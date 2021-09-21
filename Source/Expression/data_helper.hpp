#ifndef EXPRESSION_DATA_HELPER_HPP
#define EXPRESSION_DATA_HELPER_HPP

#include "evaluation_context.hpp"
#include "../Utility/function_info.hpp"

namespace expression::data {
  class Vector {
  public:
    struct Info {
      tree::Expression type;
      std::vector<tree::Expression> vec;
    };
    using Store = std::shared_ptr<Info>;
    struct Impl : DataType {
      static_assert(sizeof(Store) <= 24 && alignof(Store) <= alignof(void*));
      std::uint64_t type_index;
      std::uint64_t type_family_axiom;
      Store const& get(Buffer const& buf) const { return (Store const&)buf; }
      Store& get(Buffer& buf) const { return (Store&)buf; }
      bool compare(Buffer const& lhs, Buffer const& rhs) const override {
        return get(lhs) == get(rhs);
      }
      void copy(Buffer const& source, Buffer& target) const override {
        new (&target) Store{get(source)};
      }
      void move_destroy(Buffer& source, Buffer& target) const override {
        new (&target) Store{std::move(get(source))};
        get(source).~Store();
      }
      void destroy(Buffer& me) const override {
        get(me).~Store();
      }
      void debug_print(Buffer const& me, std::ostream& o) const override {
        o << "<vector>";
      }
      void pretty_print(Buffer const& me, std::ostream& o, mdb::function<void(tree::Expression)> sub_format) const override {
        bool first = true;
        o << "[";
        for(auto const& expr : get(me)->vec) {
          if(first) first = false;
          else o << ", ";
          sub_format(expr);
        }
        o << "]";
      }
      tree::Expression substitute(Buffer const& me, std::vector<tree::Expression> const& args) const override {
        auto const& info = *get(me);
        auto new_info = std::make_shared<Info>(Info{
          .type = expression::substitute_into_replacement(args, info.type),
          .vec = [&] {
            std::vector<tree::Expression> new_vec;
            for(auto const& expr : info.vec) {
              new_vec.push_back(expression::substitute_into_replacement(args, expr));
            }
            return new_vec;
          }()
        });
        auto ret = tree::Expression{tree::Data{.data = Data{type_index}}};
        new (&ret.get_data().data.storage) Store{std::move(new_info)};
        return ret;
      }
      void visit_children(Buffer const& me, mdb::function<void(tree::Expression const&)> visitor) const override {
        auto const& info = *get(me);
        visitor(info.type);
        for(auto const& expr : info.vec) {
          visitor(expr);
        }
      }
      tree::Expression type_of(Buffer const& me) const override {
        return tree::Apply{tree::External{type_family_axiom}, get(me)->type};
      }
    };
    std::uint64_t type_index;
    std::uint64_t type_family_axiom;
  public:
    Vector(std::uint64_t type_family_axiom):type_family_axiom(type_family_axiom) {
      auto impl = std::make_unique<Impl>();
      impl->type_family_axiom = type_family_axiom;
      auto ptr = impl.get();
      type_index = register_type(std::move(impl));
      ptr->type_index = type_index;
    }
    tree::Expression make_expression(tree::Expression type, std::vector<tree::Expression> data) const {
      auto ret = tree::Expression{tree::Data{.data = Data{type_index}}};
      new (&ret.get_data().data.storage) Store{std::make_shared<Info>(Info{
        .type = std::move(type),
        .vec = std::move(data)
      })};
      return ret;
    }
    tree::Expression operator()(tree::Expression type, std::vector<tree::Expression> data) const {
      return make_expression(std::move(type), std::move(data));
    }
    std::uint64_t get_type_family_axiom() const {
      return type_family_axiom;
    }
    std::uint64_t get_type_index() const {
      return type_index;
    }
  };
  template<class T> requires (sizeof(T) <= 24 && alignof(T) <= alignof(void*))
  class SmallScalar {
    struct Impl : DataType {
      std::uint64_t type_index;
      std::uint64_t type_axiom;
      T const& get(Buffer const& buf) const { return (T const&)buf; }
      T& get(Buffer& buf) const { return (T&)buf; }
      bool compare(Buffer const& lhs, Buffer const& rhs) const override {
        return get(lhs) == get(rhs);
      }
      void copy(Buffer const& source, Buffer& target) const override {
        new (&target) T{get(source)};
      }
      void move_destroy(Buffer& source, Buffer& target) const override {
        new (&target) T{std::move(get(source))};
        get(source).~T();
      }
      void destroy(Buffer& me) const override {
        get(me).~T();
      }
      void debug_print(Buffer const& me, std::ostream& o) const override {
        o << get(me);
      }
      void pretty_print(Buffer const& me, std::ostream& o, mdb::function<void(tree::Expression)>) const override {
        debug_print(me, o);
      }
      tree::Expression substitute(Buffer const& me, std::vector<tree::Expression> const&) const override {
        auto ret = tree::Expression{tree::Data{.data = Data{type_index}}};
        new (&ret.get_data().data.storage) T{get(me)};
        return ret;
      }
      void visit_children(Buffer const& me, mdb::function<void(tree::Expression const&)>) const override {
        //do nothing
      }
      tree::Expression type_of(Buffer const& me) const override {
        return tree::External{type_axiom};
      }
    };
    std::uint64_t type_index;
    std::uint64_t type_axiom;
  public:
    using Type = T;
    SmallScalar(Context& context) {
      auto impl = std::make_unique<Impl>();
      type_axiom = context.create_variable({
        .is_axiom = true,
        .type = tree::External{context.primitives.type}
      });
      impl->type_axiom = type_axiom;
      auto ptr = impl.get();
      type_index = register_type(std::move(impl));
      ptr->type_index = type_index;
    }
    tree::Expression make_expression(T value) const {
      auto ret = tree::Expression{tree::Data{.data = Data{type_index}}};
      new (&ret.get_data().data.storage) T{std::move(value)};
      return ret;
    }
    tree::Expression operator()(T value) const {
      return make_expression(std::move(value));
    }
    std::uint64_t get_type_axiom() const {
      return type_axiom;
    }
    std::uint64_t get_type_index() const {
      return type_index;
    }
    tree::Expression get_type_expr() const {
      return tree::External{type_axiom};
    }
    data_pattern::Pattern get_type_pattern() const {
      return data_pattern::Data{type_index};
    }
    T& extract(Buffer& buffer) const { return (T&)buffer; }
    T const& extract(Buffer const& buffer) const { return (T const&)buffer; }
  };

  namespace builder {
    template<class LHS, class RHS>
    struct Apply {
      static constexpr std::uint64_t arg_count = LHS::arg_count + RHS::arg_count;
      LHS lhs;
      RHS rhs;
      data_pattern::Pattern get_pattern() const {
        return data_pattern::Apply{
          lhs.get_pattern(),
          rhs.get_pattern()
        };
      }
      template<class Callback>
      tree::Expression call(std::vector<tree::Expression>& input, std::uint64_t base_index, Callback&& callback) const {
        return lhs.call(input, base_index, [&]<class... LArgs>(LArgs&&... largs) {
          return rhs.call(input, base_index + LHS::arg_count, [&]<class... RArgs>(RArgs&&... rargs) {
            return callback(std::forward<LArgs>(largs)..., std::forward<RArgs>(rargs)...);
          });
        });
      }
    };
    template<class LHS, class RHS> Apply(LHS, RHS) -> Apply<LHS, RHS>;
    template<class Extractor>
    struct DataMatch {
      static constexpr std::uint64_t arg_count = 1;
      std::uint64_t type_index;
      Extractor extractor;
      data_pattern::Pattern get_pattern() const {
        return data_pattern::Data{type_index};
      }
      template<class Callback>
      tree::Expression call(std::vector<tree::Expression>& input, std::uint64_t base_index, Callback&& callback) const {
        return callback(extractor(input[base_index].get_data().data.storage));
      }
    };
    template<class Extractor> DataMatch(std::uint64_t, Extractor) -> DataMatch<Extractor>;
    struct Wildcard {
      static constexpr std::uint64_t arg_count = 1;
      data_pattern::Pattern get_pattern() const {
        return data_pattern::Wildcard{};
      }
      template<class Callback>
      tree::Expression call(std::vector<tree::Expression>& input, std::uint64_t base_index, Callback&& callback) const {
        return callback(std::move(input[base_index]));
      }
    };
    struct Ignore {
      static constexpr std::uint64_t arg_count = 1;
      data_pattern::Pattern get_pattern() const {
        return data_pattern::Wildcard{};
      }
      template<class Callback>
      tree::Expression call(std::vector<tree::Expression>& input, std::uint64_t base_index, Callback&& callback) const {
        return callback();
      }
    };
    struct Fixed {
      static constexpr std::uint64_t arg_count = 0;
      std::uint64_t head;
      data_pattern::Pattern get_pattern() const {
        return data_pattern::Fixed{head};
      }
      template<class Callback>
      tree::Expression call(std::vector<tree::Expression>& input, std::uint64_t base_index, Callback&& callback) const {
        return callback();
      }
    };
    template<class T>
    auto match(SmallScalar<T> const& type) {
      return DataMatch{
        type.get_type_index(),
        [](auto& input) { return std::move((T&)input); }
      };
    }
    inline auto match(Vector const& type) {
      return DataMatch{
        type.get_type_index(),
        [](auto& input) -> std::vector<tree::Expression> const& {
          return ((Vector::Store&)input)->vec;
        }
      };
    }
    constexpr auto fixed(std::uint64_t head) {
      return Fixed{head};
    }
    constexpr auto wildcard = Wildcard{};
    constexpr auto ignore = Ignore{};
    template<class Head>
    auto pattern(Head&& head) {
      return head;
    }
    template<class Head, class Arg1, class... Args>
    auto pattern(Head&& head, Arg1&& arg1, Args&&... args) {
      return pattern(Apply{std::forward<Head>(head), std::forward<Arg1>(arg1)}, std::forward<Args>(args)...);
    }
    template<class LHS, class RHS, class Callback>
    DataRule operator>>(Apply<LHS, RHS> apply, Callback callback) {
      auto pat = apply.get_pattern();
      return DataRule{
        .pattern = std::move(pat),
        .replace = [apply = std::move(apply), callback = std::move(callback)](std::vector<tree::Expression> input) {
          return apply.call(input, 0, callback);
        }
      };
    }
    struct ManufacturedRule {
      std::uint64_t head;
      DataRule rule;
    };
    template<class... Kinds>
    struct RuleMaker {
      Context& context;
      std::tuple<Kinds...> kinds;
      RuleMaker(Context& context, Kinds... kinds):context(context), kinds(std::move(kinds)...) {}
      template<class T, std::uint64_t index>
      auto const& get_kind_for_impl() const {
        static_assert(index < sizeof...(Kinds));
        using Kind = std::decay_t<decltype(std::get<index>(kinds))>;
        if constexpr(std::is_same_v<typename Kind::Type, T>) {
          return std::get<index>(kinds);
        } else {
          return get_kind_for_impl<T, index + 1>();
        }
      }
      template<class T>
      auto const& get_kind_for() const {
        static_assert((std::is_same_v<typename Kinds::Type, T> + ...) == 1, "The selected kind must appear exactly once in the type list.");
        return get_kind_for_impl<T, 0>();
      }
      template<class T>
      tree::Expression get_type_expr() const {
        return get_kind_for<T>().get_type_expr();
      }
      template<class T>
      data_pattern::Pattern get_type_pattern() const {
        return get_kind_for<T>().get_type_pattern();
      }
      template<class T>
      decltype(auto) extract(tree::Expression& expr) const {
        return get_kind_for<T>().extract(expr.get_data().data.storage);
      }
      template<class T>
      tree::Expression embed(T&& value) const {
        return get_kind_for<T>()(std::forward<T>(value));
      }
      template<class F>
      ManufacturedRule operator()(F f) const {
        return [&]<class Ret, class... Args>(mdb::FunctionInfo<Ret(Args...)>) {
          struct TypeResult {
            Context& context;
            tree::Expression type;
            TypeResult operator*(TypeResult const& other) {
              auto codomain = multi_apply(
                tree::External{context.primitives.constant},
                tree::External{context.primitives.type},
                std::move(other.type),
                type
              );
              return TypeResult{
                .context = context,
                .type = multi_apply(
                  tree::External{context.primitives.arrow},
                  std::move(type),
                  std::move(codomain)
                )
              };
            }
          };
          auto type = (TypeResult{context, get_type_expr<Args>()} * ... * TypeResult{context, get_type_expr<Ret>()}).type; //this fold is a right-fold (x * (y * (z * w)))
          std::uint64_t head = context.create_variable({
            .is_axiom = false,
            .type = std::move(type)
          });
          data_pattern::Pattern pat = data_pattern::Fixed{head};
          ((pat = data_pattern::Apply{std::move(pat), get_type_pattern<Args>()}) , ...);
          auto replace = [this, f = std::move(f)](std::vector<tree::Expression> input) -> tree::Expression {
            return [&]<std::size_t... index>(std::index_sequence<index...>) {
              return embed(
                f(extract<Args>(input[index])...)
              );
            }(std::make_index_sequence<sizeof...(Args)>{});
          };
          return ManufacturedRule{
            .head = head,
            .rule = {
              .pattern = std::move(pat),
              .replace = std::move(replace)
            }
          };
        }(mdb::function_info_for<F>);
      }
    };
    template<class... Kinds> RuleMaker(Context&, Kinds...) -> RuleMaker<Kinds...>;

  }
}

#endif
