#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <memory>
#include <vector>
#include <variant>

struct axiom;
struct apply;
struct lambda;
struct argument;
class any_expression;

struct normalized_to_arg_t;
struct normalized_to_axiom_t;

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
    } else if constexpr(std::is_same_v<T, axiom>) {
      static vtable_t const vtable = {
        .primitive_type = 4,
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
  std::size_t& get_ref_count() const;
  vtable_t const* get_vptr() const;
  void* get_data_ptr() const;
  template<class T>
  T* get_data_ptr_as() const { return (T*)get_data_ptr(); }
public:
  any_expression(any_expression const& other);
  any_expression(any_expression&& other);
  any_expression& operator=(any_expression const& other);
  any_expression& operator=(any_expression&& other);
  template<class T, class base_type = std::remove_cv_t<std::remove_reference_t<T> > >
  any_expression(T&& value) requires (std::is_same_v<base_type, apply> || std::is_same_v<base_type, lambda> || std::is_same_v<base_type, argument> || std::is_same_v<base_type, axiom> || reducible_expression<base_type>):
  data((std::uint8_t*)malloc(sizeof(std::size_t) + sizeof(vtable_t*) + sizeof(base_type))) {
    vtable_t const* vtable = get_vtable_for_type<base_type>();
    new (data) std::size_t(1); //ref count
    new (data + sizeof(std::size_t)) vtable_t const*(vtable);
    new (data + sizeof(std::size_t) + sizeof(vtable_t*)) base_type(std::forward<T>(value));
  }
  std::size_t get_primitive_type() const;
  any_expression step_towards_primitive() const;
  any_expression reduce_to_primitive() const;
  template<class callback_t, class ret_type = std::common_type_t<std::invoke_result_t<callback_t, axiom const&>, std::invoke_result_t<callback_t, apply const&>, std::invoke_result_t<callback_t, lambda const&>, std::invoke_result_t<callback_t, argument const&> > >
  ret_type reduce_to_primitive_then(callback_t&& callback) const {
    any_expression reduced = reduce_to_primitive();
    switch(reduced.get_primitive_type()) {
      case 1: return callback(*reduced.get_data_ptr_as<apply const>());
      case 2: return callback(*reduced.get_data_ptr_as<lambda const>());
      case 3: return callback(*reduced.get_data_ptr_as<argument const>());
      case 4: return callback(*reduced.get_data_ptr_as<axiom const>());
      default: std::terminate();
    }
  }
  any_expression normalize_locally() const;
  any_expression normalize_globally() const;
  template<class callback_t, class ret_type = std::common_type_t<std::invoke_result_t<callback_t, normalized_to_axiom_t>, std::invoke_result_t<callback_t, normalized_to_arg_t>, std::invoke_result_t<callback_t, lambda const&> > >
  ret_type normalize_locally_then(callback_t&& callback) const;
  ~any_expression();
};
//using normalized_to_axiom_t = any_expression::normalized_to_axiom_t;
//using normalized_to_arg_t = any_expression::normalized_to_arg_t;

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
struct axiom {
  std::size_t index;
};

struct normalized_to_axiom_t { axiom a; std::vector<any_expression> rev_args; };
struct normalized_to_arg_t { argument a; std::vector<any_expression> rev_args; };
template<class callback_t, class ret_type>
ret_type any_expression::normalize_locally_then(callback_t&& callback) const {
 any_expression normalized = normalize_locally();
 if(normalized.get_primitive_type() == 2) return callback(*normalized.get_data_ptr_as<lambda const>());
 std::vector<any_expression> rev_args;
 while(normalized.get_primitive_type() == 1) {
   auto const& application = *normalized.get_data_ptr_as<apply const>();
   rev_args.push_back(application.x);
   normalized = application.f;
 }
 if(normalized.get_primitive_type() == 3) return callback(normalized_to_arg_t{*normalized.get_data_ptr_as<argument const>(), std::move(rev_args)});
 if(normalized.get_primitive_type() == 4) return callback(normalized_to_axiom_t{*normalized.get_data_ptr_as<axiom const>(), std::move(rev_args)});
 std::terminate(); //??? it's not normalized
};

std::ostream& operator<<(std::ostream&, any_expression const&);
any_expression totally_simplify(any_expression const& e);
struct bind_expression {
  std::size_t first_depth;
  std::shared_ptr<std::vector<any_expression> > bound;
  any_expression target;
  any_expression reduce() const;
};
struct deepened_expression {
  std::size_t ignored_args;
  std::size_t bound_depth;
  any_expression target;
  any_expression reduce() const;
};
struct substitute_expression {
  std::size_t depth;
  any_expression arg;
  any_expression target;
  any_expression reduce() const;
};
bool deep_compare(any_expression lhs, any_expression rhs);

#endif
