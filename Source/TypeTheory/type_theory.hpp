/*

#ifndef TYPE_THEORY_HPP
#define TYPE_THEORY_HPP

#include "../Utility/shared_variant.hpp"
#include "../Utility/overloaded.hpp"
#include <vector>

namespace Type {
  struct Nat;
  struct Product;
  struct Array;

  using Any = SharedVariant<Nat, Product, Array>;
};
namespace Value {
  struct Apply;
  struct Zero;
  struct Succ;
  struct Lambda;
  struct Empty;
  struct Cons;
  struct Induct;
  struct Arg;

  using Any = SharedVariant<Apply, Zero, Succ, Lambda, Empty, Cons, Induct, Arg>;
};

namespace Type {
  struct Nat {};
  struct Product {
    Any carrier;
    Any target;
  };
  struct Array {
    Any base;
    Value::Any count;
  };
};
namespace Value {
  struct Apply {
    Any f;
    Any x;
  };
  struct Zero {};
  struct Succ {
    Any value;
  };
  struct Lambda {
    Any body;
  };
  struct Empty {};
  struct Cons {
    Any head;
    Any tail;
  };
  struct Induct {
    Any step;
    Any base;
    Any count;
  };
  struct Arg {
    std::size_t co_index;
  };
}

struct ContextMorphism {
  std::vector<Value::Any> substitutions;
  std::size_t codomain_depth; //depth of context
  Value::Any get(std::size_t co_index) const {
    if(co_index >= substitutions.size()) {
      throw std::runtime_error("Argument index out of range.");
    } else {
      return substitutions.at(substitutions.size() - co_index - 1);
    }
  }
};

Type::Any context_map(ContextMorphism const& morphism, Type::Any const& type);
Value::Any context_map(ContextMorphism const& morphism, Value::Any const& type);
Value::Any full_simplify(std::size_t context_depth, Value::Any const& value);
std::ostream& operator<<(std::ostream& o, Value::Any const& v);

namespace Helper {
  namespace Detail {
    template<class T>
    constexpr auto wrap_value_constructor = []<class... Args>(Args&&... args) { return Value::Any{ T{std::forward<Args>(args)...} }; };
    template<class T>
    constexpr auto wrap_value_singleton = []() { static Value::Any ret = T{}; return ret; };
  }
  inline Value::Any apply(Value::Any f, Value::Any x) {
    return Value::Apply{f, x};
  }
  template<class... Args> requires (std::is_convertible_v<Args, Value::Any> && ...)
  inline Value::Any apply(Value::Any f, Value::Any x1, Value::Any x2, Args&&... args) {
    return apply(apply(f, x1), x2, std::forward<Args>(args)...);
  }
  inline Value::Any lambda(Value::Any body) {
    return Value::Lambda{body};
  }
  inline Value::Any induct(Value::Any step, Value::Any base, Value::Any count) {
    return Value::Induct{step, base, count};
  }
  constexpr Value::Zero zero{};
  inline Value::Any succ(Value::Any value) {
    return Value::Succ{value};
  }
  inline Value::Any arg(std::size_t index) {
    return Value::Arg{index};
  }
  constexpr Value::Empty empty{};
};

#endif

*/
