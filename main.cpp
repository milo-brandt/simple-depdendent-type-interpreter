//#include "Runtime/builder.hpp"
#include <iostream>
#include <cstdint>
#include <concepts>
#include <cassert>
#include <cstring>
#include "Templates/TemplateUtility.h"


struct apply;
struct lambda;
struct argument;
class any_expression;
template<class T>
concept reducible_expression = requires(T const& a) { {a.reduce()} -> std::convertible_to<any_expression>; };
class any_expression {
  std::uint8_t* data;
  struct vtable_t {
    std::size_t primitive_type;
    any_expression(*reduce)(void*);
    void(*destroy)(void*);
  };
  template<class T>
  static vtable_t const* get_vtable_for_type() {
    auto destroyer = [](void* ptr){ ((T*)ptr)->~T(); };
    if constexpr(std::is_same_v<T, apply>) {
      static vtable_t const vtable = {
        .primitive_type = 1,
        .destroy = destroyer
      };
      return &vtable;
    } else if constexpr(std::is_same_v<T, lambda>) {
      static vtable_t const vtable = {
        .primitive_type = 2,
        .destroy = destroyer
      };
      return &vtable;
    } else if constexpr(std::is_same_v<T, argument>) {
      static vtable_t const vtable = {
        .primitive_type = 3,
        .destroy = destroyer
      };
      return &vtable;
    } else {
      static vtable_t const vtable = {
        .primitive_type = 0,
        .reduce = [](void* ptr) -> any_expression{ return ((T*)ptr)->reduce(); },
        .destroy = destroyer
      };
      return &vtable;
    }
  }
  std::size_t& get_ref_count() const { return *(std::size_t*)data; }
  vtable_t const* get_vptr() const { return *(vtable_t**)(data + sizeof(std::size_t)); }
  void* get_data_ptr() const { return data + sizeof(std::size_t) + sizeof(vtable_t*); }
  template<class T>
  T* get_data_ptr_as() const { return (T*)get_data_ptr(); }
public:
  any_expression(any_expression const& other):data(other.data) {
    if(data) ++get_ref_count();
  }
  any_expression(any_expression&& other):data(other.data) {
    other.data = nullptr;
  }
  any_expression& operator=(any_expression const& other) {
    any_expression copy{other};
    this->~any_expression();
    new (this) any_expression(std::move(copy));
    return *this;
  }
  any_expression& operator=(any_expression&& other) {
    std::swap(data, other.data);
    return *this;
  }
  template<class T, class base_type = std::remove_cv_t<std::remove_reference_t<T> > >
  any_expression(T&& value) requires (std::is_same_v<base_type, apply> || std::is_same_v<base_type, lambda> || std::is_same_v<base_type, argument> || reducible_expression<base_type>)
  :data((std::uint8_t*)malloc(sizeof(std::size_t) + sizeof(vtable_t*) + sizeof(base_type))) {
    vtable_t const* vtable = get_vtable_for_type<base_type>();
    new (data) std::size_t(1); //ref count
    new (data + sizeof(std::size_t)) vtable_t const*(vtable);
    new (data + sizeof(std::size_t) + sizeof(vtable_t*)) base_type(std::forward<T>(value));
  }
  std::size_t get_primitive_type() const { return get_vptr()->primitive_type; }
  any_expression reduce_once() const{
    assert(!get_primitive_type());
    return get_vptr()->reduce(get_data_ptr());
  }
  any_expression reduce_all() const {
    any_expression ret = *this;
    while(!ret.get_primitive_type()) ret = ret.reduce_once();
    return ret;
  }
  template<class callback_t, class ret_type = std::common_type_t<std::invoke_result_t<callback_t, apply const&>, std::invoke_result_t<callback_t, lambda const&>, std::invoke_result_t<callback_t, argument const&> > >
  ret_type reduce_and_visit(callback_t&& callback) const {
    any_expression reduced = reduce_all();
    switch(reduced.get_primitive_type()) {
      case 1: return callback(*reduced.get_data_ptr_as<apply const>());
      case 2: return callback(*reduced.get_data_ptr_as<lambda const>());
      case 3: return callback(*reduced.get_data_ptr_as<argument const>());
      default: std::terminate();
    }
  }
  ~any_expression() {
    if(data && --get_ref_count() == 0) {
      get_vptr()->destroy(get_data_ptr());
      free(data);
    }
  }
};
struct apply {
  any_expression f;
  any_expression x;
};
struct lambda {
  any_expression body;
};
struct argument {
  std::size_t arg;
};
void output_expr(any_expression const& e);
#include <memory>
#include <vector>

struct bind_expression {
  std::size_t first_depth;
  std::shared_ptr<std::vector<any_expression> > bound;
  any_expression target;
  any_expression reduce() const {
    return target.reduce_and_visit(utility::overloaded{
      [&](apply const& a) -> any_expression {
        return apply{ bind_expression{ first_depth, bound, a.f } , bind_expression{ first_depth, bound, a.x } };
      },
      [&](lambda const& l) -> any_expression {
        return lambda{ bind_expression{ first_depth + 1, bound, l.body } };
      },
      [&](argument const& a) -> any_expression {
        if(a.arg > first_depth) return argument{ a.arg - first_depth };
        if(first_depth - a.arg >= bound->size()) return argument{ a.arg };
        return (*bound)[first_depth - a.arg];
      }
    });
  }
};
struct deepened_expression {
  std::size_t ignored_args;
  std::size_t bound_depth;
  any_expression target;
  any_expression reduce() const {
    return target.reduce_and_visit(utility::overloaded{
      [&](apply const& a) -> any_expression {
        return apply{ deepened_expression{ ignored_args, bound_depth, a.f } , deepened_expression{ ignored_args, bound_depth, a.x } };
      },
      [&](lambda const& l) -> any_expression {
        return lambda{ deepened_expression{ ignored_args, bound_depth + 1, l.body } };
      },
      [&](argument const& a) -> any_expression {
        if(a.arg < bound_depth) return a;
        else return argument { a.arg + ignored_args };
      }
    });
  }
};
template<class T>
constexpr T round_up_to(T value, T step) {
  return ((value - 1)/step + 1) * step;
}

/*
template<class T>
class shared_stack {
  class head {
    std::size_t length;
    std::size_t capacity;
  };
  static constexpr std::size_t byte_offset = round_up_to(sizeof(head), alignof(T));
  std::uint8_t* data;
  std::size_t length_used;
  head* get_head() { return (head*)data; }
  T* get_data_start() { return (T*)(data + byte_offset); }
  T* get_data_at(std::size_t pos){ return get_data_start() + pos; }
  std::size_t* get_ref_count(std::size_t where) { return get_head()->separation + where; }
  void release() {
    if(--get_ref_count(length_used) == 0 && (get_head()->end - get_data_start()) == length_used) {
      while(true) { //destruct stack up to next point with reference; if no more references exist, free the allocation
        if(length_used == 0) {
          free(data);
        }
        get_data_at(--length_used)->~T();
        if(get_ref_count(length_used) > 0) {
          get_head()->length = length_used;
          return;
        }
      }
    }
  }
  static std::uint8_t* allocate_for(std::size_t count) {
    std::size_t ref_offset = round_up_to(sizeof(T) * count, alignof(std::size_t));
    std::size_t ref_size = sizeof(std::size_t) * count;
    std::size_t allocation_size = ref_offset + ref_size;
    std::uint8_t* ret = (std::uint8_t*)malloc(allocation_size);
    memset(ret + ref_offset, 0,  ref_size); //zero memory
    return ret;
  }
public:
  ~shared_stack() {
    if(data) release();
  }
};
*/


struct substitute_expression {
  std::size_t depth;
  any_expression arg;
  any_expression target;
  any_expression reduce() const {
    return target.reduce_and_visit(utility::overloaded{
      [&](apply const& a) -> any_expression {
        return apply{ substitute_expression{ depth, arg, a.f } , substitute_expression{ depth, arg, a.x } };
      },
      [&](lambda const& l) -> any_expression {
        return lambda{ substitute_expression{ depth + 1, arg, l.body } };
      },
      [&](argument const& a) -> any_expression {
        if(a.arg < depth) return a;
        else if(a.arg > depth) return argument{ a.arg - 1 };
        else if(depth == 0) {
          return arg;
        } else {
          return deepened_expression{ depth, 0, arg };
        }
      }
    });
  }
};

void output_expr(any_expression const& e) {
  e.reduce_and_visit(utility::overloaded{
    [&](apply const& a) {
      std::cout << "apply("; output_expr(a.f); std::cout << ", "; output_expr(a.x); std::cout << ")";
    },
    [&](lambda const& l) {
      std::cout << "lambda("; output_expr(l.body); std::cout << ")";
    },
    [&](argument const& a) {
      std::cout << "arg(" << a.arg << ")";
    }
  });
}
any_expression totally_simplify(any_expression const& e) {
  return e.reduce_and_visit(utility::overloaded{
    [&](apply const& a) -> any_expression {
      return totally_simplify(a.f).reduce_and_visit(utility::overloaded{
        [&](apply const& aa) -> any_expression {
          return apply{aa, totally_simplify(a.x)};
        },
        [&](lambda const& l) -> any_expression {
          return totally_simplify(substitute_expression{0, a.x, l.body});
        },
        [&](argument const& aa) -> any_expression {
          return apply{aa, totally_simplify(a.x)};
        }
      });
    },
    [&](lambda const& l) -> any_expression {
      return lambda{totally_simplify(l.body)};
    },
    [&](argument const& a) -> any_expression {
      return a;
    }
  });
}
/*
namespace reduction {
  struct abstract_reduction_unit {

  }
};*/
/*
struct deepened_expression2 {
  std::size_t ignored_args;
  std::size_t bound_depth;
  any_expression target;
  auto reduce() const {
    return fully_reduce_and_then {
      [&](apply const& a) {

      },
      [&](lambda const& l) {

      },
      [&](argument const& a) {

      },
      [&](on_failure const& e) {
        return deepened_expression2{ignored_args, bound_depth, e.best};
      }
    };
    return target.reduce_and_visit(utility::overloaded{
      [&](apply const& a) -> any_expression {
        return apply{ deepened_expression{ ignored_args, bound_depth, a.f } , deepened_expression{ ignored_args, bound_depth, a.x } };
      },
      [&](lambda const& l) -> any_expression {
        return lambda{ deepened_expression{ ignored_args, bound_depth + 1, l.body } };
      },
      [&](argument const& a) -> any_expression {
        if(a.arg < bound_depth) return a;
        else return argument { a.arg + ignored_args };
      }
    });*/
/*  }
};



struct evaluation_context {
  any_expression simplify(any_expression const& input);
};*/

/*
Goal:
  Be able to write a storage type that takes a sized storage type as an input and represents an array.

e.g. if we have a Uint64Natural storage class, we should be able to also have a Array(Uint64Natural, 17) class and a Array(Array(Uint64Natural, 17), 13) class
*/

/*
  AnyExpression simplify(void* type_data, void* instance_data) {

  }
*/
/*
struct AnyExpression;
struct TypeHead {
  std::size_t primitive_type;
  AnyExpression(*normalize)(void* type_data, void* instance_data);
  void(*destroy_instance)(void* type_data, void* instance_data);
  void(*destroy_type)(void* type_data);
};
struct AnyExpression {
  void* type_data; //TypeHead then...
  void* instance_data;
};

void* make_weak_type(void* type_data) {
  struct WeakTypeTail {
    TypeHead head;
    void* type_data;
  };
  TypeHead head{
    .normalize = [](void* type_data, void* instance_data) {
      auto inner_type = (TypeHead*)((WeakTypeTail*)type_data)->type_data;
      return inner_type->normalize(inner_type, instance_data);
    },
    .destroy_instance = [](void*,void*){},
    .destroy_type = [](void* t){ free(t); }
  };
}
void* make_array_type(void* type_data, std::size_t stride, std::size_t count) {
  struct ArrayTypeTail {
    TypeHead head;
    std::size_t stride;
    std::size_t count;
    void* inner_type;
  };
  auto normalizer = [](void* type_data, void* instance_data) {
    auto array_type_data = (ArrayTypeTail*)type_data;
    std::vector<AnyExpression> exprs;
    for(std::size_t i = 0; i < array_type_data->count; ++i) {
      exprs.push_back(AnyExpression{ array_type_data->inner_type, instance_data + array_type_data->stride * i });
    }
  };
}*/

/*
#include <coroutine>
#include <variant>

struct WaitForToken{};
constexpr WaitForToken wait_for_token;
struct DoneStreamTransformer{};
template<class InType, class OutType>
class WaitingStreamTransformer;
template<class InType, class OutType>
class ReadyStreamTransformer;
template<class InType, class OutType>
using StreamResult = std::variant<DoneStreamTransformer, std::pair<OutType, ReadyStreamTransformer<InType, OutType> >, WaitingStreamTransformer<InType, OutType> >;


template<class InType, class OutType>
class WaitingStreamTransformer {
public:
  StreamResult<InType, OutType> advance(InType) &&;
};
template<class InType, class OutType>
class ReadyStreamTransformer {
public:
  struct promise_type;
  StreamResult<InType, OutType> advance() &&;
private:
  std::coroutine_handle<promise_type> handle;
  ReadyStreamTransformer(std::coroutine_handle<promise_type> handle):handle(handle){}
};

template<class InType, class OutType>
struct ReadyStreamTransformer<InType, OutType>::promise_type {
  struct TokenAwaiter {
    std::optional<InType> token;
    bool await_ready() const { return token.has_value(); }
    void await_suspend(std::coroutine_handle<>) const {}
    InType await_resume() {
      assert(token);
      return std::move(*token);
    }
  };
  TokenAwaiter token_awaiter;
  std::unique_ptr<OutType> value = nullptr;
  std::exception_ptr exception = nullptr;
  ReadyStreamTransformer get_return_object() {
    return ReadyStreamTransformer(std::coroutine_handle<promise_type>::from_promise(*this));
  }
  constexpr std::suspend_always initial_suspend() const noexcept{ return {}; }
  constexpr std::suspend_always final_suspend() const noexcept{ return {}; }
  constexpr TokenAwaiter& await_transform(WaitForToken) const noexcept{ return token_awaiter; }
  std::suspend_always yield_value(std::remove_reference_t<OutType>& value) requires(!std::is_rvalue_reference_v<OutType>) {
    value = std::addressof(value);
    return {};
  }
  std::suspend_always yield_value(std::remove_reference_t<OutType>&& value) {
    value = std::addressof(value);
    return {};
  }
  void unhandled_exception() {
    exception = std::current_exception();
  }
  void return_void(){}
  void destroy() {
    std::coroutine_handle<promise_type>::from_promise(*this).destroy();
  }
  void set_next_token(InType token) {
    token_awaiter.token = std::move(token);
  }
  StreamResult<InType, OutType> step() {
    std::coroutine_handle<promise_type>::from_promise(*this).resume();
    if(exception) std::rethrow_exception(exception);
    if(value) {
      auto ret = std::move(*value);
      value.reset();
      return std::make_pair(std::move(ret), ReadyStreamTransformer(std::coroutine_handle<promise_type>::from_promise(*this)));
    }
    if(std::coroutine_handle<promise_type>::from_promise(*this).done()) {
    }
  }
};
*/


/*
declare {
  Fancy: Type;
  empty: Fancy;
  append: f: Fancy -> List(Action f) -> Fancy;

  Action: Fancy -> Type;
  Action empty := 1;
  Action (append f a) := (Action f) + 1;
}
*/
//Maybe should also allow "here's a set of axioms, a method to compute them and a method to construct universal maps"
//Verification: append involes the half-positive term Action f - must see that Action is an appropriate definition (i.e. recurses to previous generations)

template<class T>
struct Parser {
  virtual void parse(std::string_view);
};
template<class T, class Continuation>
struct ReadTokenThen {
  Continuation continuation; //Continuation should be map U8 -> Parser
};
/*
  x <- readToken;
  y <- readToken;
  return x + y;


ReadTokenThen{[=](int x){
  return ReadTokenThen{[=](int y){
    return Return{x + y};
  }}
};
*/


int main() {
  /*
  any_expression a(apply{argument{0}, argument{1}});
  any_expression b = a;
  any_expression doubler = lambda{any_expression{lambda{apply{argument{1}, apply{argument{1}, argument{0}}}}}};
  any_expression quad = apply{doubler, doubler};
  any_expression twofitysix = apply{quad, quad};

  output_expr(twofitysix); std::cout << "\n";
  output_expr(totally_simplify(twofitysix)); std::cout << "\n";*/
  //output_expr(totally_simplify(doubler)); std::cout << "\n";
}
