#ifndef expression_tree_hpp
#define expression_tree_hpp

#include <variant>
#include <string>
#include <optional>
#include <ostream>
#include <functional>
#include <memory>

namespace type_theory {
  struct primitive;
  struct application;
  struct lambda;
  struct argument;
  using node = std::variant<primitive, application, lambda, argument>;

  class expression {
    std::optional<std::size_t> index;
    struct detail;
  public:
    expression() noexcept;
    expression(expression const&) noexcept;
    expression(expression&&) noexcept;
    expression(application) noexcept;
    expression(lambda) noexcept;
    expression(argument) noexcept;
    expression(primitive) noexcept;
    ~expression() noexcept;
    expression& operator=(expression const&) noexcept;
    expression& operator=(expression&&) noexcept;
    bool has_value() const;
    operator bool() const;
    node read() const;
    bool operator==(expression const&) const;
    expression operator()(expression x) const;
    template<class... Args>
    expression operator()(expression x, Args&&... args) const{
      return (*this)(x)(std::forward<Args>(args)...);
    }
    expression get_simplest_existing_form() const;
    bool is_constant_in(std::size_t index) const;
    bool is_constant_below(std::size_t index) const; //if every arg is less than this one
    friend std::pair<expression, bool> left_canonical_form(expression);
    friend expression full_canonical_form(expression);
  };
  expression beta_substitution(expression lambda_body, expression const& arg);
  expression substitute_all_args(expression body, std::vector<expression> const& args);
  std::pair<expression, bool> left_canonical_form(expression);
  expression full_canonical_form(expression);
  struct application{
    expression f;
    expression x;
  };
  struct lambda{
    std::string arg_name;
    expression body;
  };
  struct argument{
    std::size_t stack_index;
  };
  std::ostream& operator<<(std::ostream&, expression const&);
/*
stuff for primitives
*/
  struct pattern_function_def;
  namespace primitives{
    using basic = std::variant<long, bool>;
    struct external_function{
      std::string name;
      std::size_t args_required;
      expression(*f)(std::vector<basic>);
      bool operator==(external_function const&) const = default;
      bool operator!=(external_function const&) const = default;
    };
    struct exact_comparison_function_t{
      bool operator==(exact_comparison_function_t const& other) const = default;
      bool operator!=(exact_comparison_function_t const& other) const = default;
    };
    constexpr exact_comparison_function_t exact_comparison_function;
    struct pattern_function{
      std::shared_ptr<pattern_function_def> def;
      bool operator==(pattern_function const& other) const = default;
      bool operator!=(pattern_function const&) const = default;
    };
    struct constructor{
      std::string name;
      std::size_t index;
      bool operator==(constructor const&) const = default;
      bool operator!=(constructor const&) const = default;
    };
    enum class marker{
      type,
      function
    };
  }
  struct primitive{
    std::variant<primitives::basic, primitives::external_function, primitives::marker, primitives::constructor, primitives::pattern_function, primitives::exact_comparison_function_t> value;
    bool operator==(primitive const&) const = default;
    bool operator!=(primitive const& o) const{ return !(*this == o); }
  };
  struct pattern_function_pat{
    primitive head;
    std::vector<std::shared_ptr<pattern_function_pat> > sub_patterns; //pts are null if anything should be captured;
  };
  struct pattern_function_case{
    std::vector<std::shared_ptr<pattern_function_pat> > pattern;
    expression replacement; //beta_substitution will repeatedly be called on the replacement for each element of the vector.
  };
  struct pattern_function_def{
    std::string name;
    std::vector<pattern_function_case> cases;
  };
  namespace helper{
    inline expression create(expression e){ return e; }
    inline expression create(primitives::basic p){ return primitive{std::move(p)}; }
    inline expression create(primitives::external_function p){ return primitive{std::move(p)}; }
    inline expression create(primitives::pattern_function p){ return primitive{std::move(p)}; }
    inline expression create(primitives::constructor p){ return primitive{std::move(p)}; }
    inline expression create(primitives::marker p){ return primitive{std::move(p)}; }
    struct __fold_creator{
      expression value;
      __fold_creator operator*(__fold_creator val)&& {
        return __fold_creator{application{std::move(value), std::move(val.value)}};
      }
    };
    template<class first, class... Ts>
    expression create(std::tuple<first, Ts...> tup){
      return std::apply([](auto&&... args){ return ( ... * __fold_creator{create(std::move(args))}).value; }, std::move(tup));
    }
    template<class first, class second, class... Ts>
    expression create(first f, second s, Ts... t){
      return create(std::make_tuple(f,s,t...));
    }
    template<class T>
    struct lambda{
      std::string arg_name;
      T value;
    };
    template<class T> lambda(std::string, T) -> lambda<T>;
    template<class T>
    expression create(lambda<T> l) {
      return type_theory::lambda{l.arg_name, create(l.value)};
    }
  };
}

#endif
