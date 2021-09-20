#ifndef EXPRESSION_DATA_HELPER_HPP
#define EXPRESSION_DATA_HELPER_HPP

#include "evaluation_context.hpp"

namespace expression::data {
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
      tree::Expression substitute(Buffer const& me, mdb::function<tree::Expression(std::uint64_t)>) const override {
        auto ret = tree::Expression{tree::Data{.data = Data{type_index}}};
        new (&ret.get_data().data.storage) T{get(me)};
        return ret;
      }
      tree::Expression type_of(Buffer const& me) const override {
        return tree::External{type_axiom};
      }
    };
    std::uint64_t type_index;
    std::uint64_t type_axiom;
  public:
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
  };
  template class SmallScalar<std::uint64_t>;

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
  }
}

#endif
