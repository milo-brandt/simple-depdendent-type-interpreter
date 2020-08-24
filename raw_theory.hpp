#ifndef raw_theory_hpp
#define raw_theory_hpp

#include <memory>
#include "any.hpp"
#include <variant>
#include "Templates/TemplateUtility.h"
#include <deque>
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace type_theory::raw{

  struct lambda_base;
  struct argument;
  struct primitive;
  struct application;
  struct built_in;

  using term = std::shared_ptr<std::variant<built_in, lambda_base, argument, primitive, application> >;

  struct lambda_base{
      term body;
      std::size_t arg_index;
  };
  struct argument{
      std::size_t arg_index;
  };
  struct primitive{
      utility::printable_any value;
  };
  struct application{
      term f;
      term x;
  };
  struct built_in{
      struct require_values;
      struct nothing_to_do_t{};
      static constexpr nothing_to_do_t nothing_to_do{};
      struct simplified{
          std::size_t arguments_read;
          term result;
      };
      using evaluation_result = std::variant<require_values, nothing_to_do_t, simplified>;
      using evaluator = std::function<evaluation_result(std::deque<term> const& args)>;
      struct require_values{
          std::vector<std::size_t> arguments_needed;
          evaluator eval;
      };
      evaluator eval;
      std::string name;
  };
  struct name_scope{
      std::unordered_set<std::size_t> used_names;
      bool contains(std::size_t name) const;
      void add_name(std::size_t name);
      void erase_name(std::size_t name);
      void ensure_name(std::size_t name);
  };
  template<class... Args>
  term make_term(Args&&... args){
      return std::make_shared<std::variant<built_in, lambda_base, argument, primitive, application> >(std::forward<Args>(args)...);
  }
  term make_lambda(term body, std::size_t arg_index);
  term make_argument(std::size_t arg_index);
  term make_primitive(utility::printable_any value);
  term make_application(term f, term x);
  term make_application(term f, std::deque<term> const& args);
  template<class T, class... Ts> requires (sizeof...(Ts) > 0)
  term make_application(term f, T&& first, Ts&&... terms){
    return make_application(make_application(f, first), std::forward<Ts>(terms)...);
  }
  term make_builtin(built_in::evaluator f, std::string name);
  term make_formal(std::string name);

  std::pair<term, std::deque<term> > extract_applications(term t);
  term sanitize_term(term base, name_scope used_names);
  term apply_lambda(lambda_base const& f, term arg, name_scope const& s = {});
  std::pair<term, bool> try_simplify_at_top(term t, name_scope const& scope = {});
  std::ostream& operator<<(std::ostream& o, term t);
  term full_simplify(term t, name_scope const& names = {});
  bool compare_literal(term a, term b, name_scope const& names = {});
}

#endif
