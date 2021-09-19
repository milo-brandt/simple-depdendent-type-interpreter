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
    std::uint64_t get_type_axiom() {
      return type_axiom;
    }
    std::uint64_t get_type_index() {
      return type_index;
    }
  };
  template class SmallScalar<std::uint64_t>;
}

#endif
