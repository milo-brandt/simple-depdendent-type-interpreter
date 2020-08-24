#include <iostream>
#include "typed_theory.hpp"

#include <optional>
using namespace type_theory;

namespace helpers{
  struct lambda_expression;
  struct application;
  struct argument;
  class expression{
  public:
    using storage_type = std::variant<lambda_expression, application, typed_term, argument>;
    storage_type& get();
    storage_type& get() const;
    storage_type* operator->();
    storage_type const* operator->() const;
    storage_type& operator*();
    storage_type const& operator*() const;
    expression operator()(expression b) const;
    template<class... T> requires (sizeof...(T) > 0)
    expression operator()(expression b, T... vals){ return (*this)(std::move(b))(std::forward<T>(vals)...); }
    expression(lambda_expression v);
    expression(application v);
    expression(argument v);
    expression(typed_term v);
    expression(raw::term type, raw::term value):expression(typed_term{type, value}){}
    expression(expression a, raw::term b):expression(a.compile().value, b){}
    typed_term compile() const;
    operator typed_term() const{ return compile(); }
  private:
    struct compile_data{
      struct arg_data{
        std::size_t index;
        raw::term type;
      };
      std::unordered_map<std::string, arg_data> arg_indices;
      std::unordered_set<std::size_t> used;
    };
    typed_term compile_impl(compile_data& data) const;
    std::shared_ptr<storage_type> value;
  };
  struct argument{
    std::string arg_name;
    expression operator()(expression a) const{
      return expression(*this)(a);
    }
  };
  struct lambda_expression{
    std::string arg_name;
    expression arg_type;
    expression body;
    expression operator()(expression a) const{
      return expression(*this)(a);
    }
  };
  struct application{
    expression f;
    expression x;
    expression operator()(expression a) const{
      return expression(*this)(a);
    }
  };
  struct named{
    std::string name;
    expression type;
  };
  typed_term get_type_t(){
    auto raw_type = raw::make_formal("type");
    return {raw_type, raw_type};
  }
  typed_term get_function_head(){
    auto type = get_type_t();
    auto raw_func = raw::make_formal("function");
    return {raw::make_application(
      raw_func,
      type.value,
      raw::make_lambda(raw::make_application(
        raw_func,
        raw::make_application(
          raw_func,
          raw::make_argument(0),
          raw::make_lambda(
            type.value
          ,1)
        ),
        raw::make_lambda(
          type.value
        ,1)
      ),0)
    ), raw_func};
  }
  expression func_type(expression last){
    return last;
  }
  template<class... T>
  expression func_type(expression arg, T&&... rest){
    return expression(application{
      application{
        get_function_head(),
        arg
      },
      lambda_expression{
        "",
        arg,
        func_type(std::forward<T>(rest)...)
      }
    });
  }
  template<class... T>
  expression func_type(named arg, T&&... rest){
    return expression(application{
      application{
        get_function_head(),
        arg.type
      },
      lambda_expression{
        arg.name,
        arg.type,
        func_type(std::forward<T>(rest)...)
      }
    });
  }
}
#include <sstream>
namespace helpers{
  namespace{
    template<class... Args>
    std::string convert_and_concatenate(Args&&... args){
      std::stringstream s;
      (s << ... << std::forward<Args>(args));
      return s.str();
    }
  }

  expression::expression(lambda_expression v):value(std::make_shared<storage_type>(v)){}
  expression::expression(application v):value(std::make_shared<storage_type>(v)){}
  expression::expression(argument v):value(std::make_shared<storage_type>(v)){}
  expression::expression(typed_term v):value(std::make_shared<storage_type>(v)){}
  expression::storage_type& expression::get(){ return *value; }
  expression::storage_type& expression::get() const{ return *value; }
  expression::storage_type* expression::operator->(){ return &get(); }
  expression::storage_type const* expression::operator->() const{ return &get(); }
  expression::storage_type& expression::operator*(){ return get(); }
  expression::storage_type const& expression::operator*() const{ return get(); }
  expression expression::operator()(expression b) const{
    return expression(application{*this, b});
  }
  typed_term expression::compile() const{
    compile_data data;
    return compile_impl(data);
  }
  namespace{
    std::size_t choose_not_from_set(std::unordered_set<std::size_t>& s){
      while(true){
        std::size_t candidate = rand();
        if(!s.contains(candidate)){
          s.insert(candidate);
          return candidate;
        }
      }
    }
  }
  typed_term expression::compile_impl(compile_data& data) const{
    return std::visit(utility::overloaded{
      [&](lambda_expression const& e) -> typed_term{
        auto arg_type = full_simplify(e.arg_type.compile_impl(data));
        if(!std::holds_alternative<raw::built_in>(*arg_type.type) || std::get<raw::built_in>(*arg_type.type).name != "type")
          throw std::runtime_error(convert_and_concatenate("type given for lambda argument is not type; given value was ",arg_type));
        auto index = choose_not_from_set(data.used);
        std::optional<compile_data::arg_data> old;
        if(data.arg_indices.contains(e.arg_name)) old = data.arg_indices.at(e.arg_name);
        data.arg_indices[e.arg_name] = {index, arg_type.value};
        auto ret = e.body.compile_impl(data);
        if(old)
          data.arg_indices[e.arg_name] = *old;
        else
          data.arg_indices.erase(e.arg_name);
        data.used.erase(index);
        return {raw::make_application(raw::make_formal("function"),arg_type.value,raw::make_lambda(ret.type,index)), raw::make_lambda(ret.value, index)};
      },
      [&](application const& a){
        return safe_apply(a.f.compile_impl(data), a.x.compile_impl(data));
      },
      [&](argument const& arg) -> typed_term{
        if(!data.arg_indices.contains(arg.arg_name))
          throw std::runtime_error("unrecognized argument: " + arg.arg_name);
        auto const& arg_info = data.arg_indices.at(arg.arg_name);
        return {arg_info.type, raw::make_argument(arg_info.index)};
      },
      [&](typed_term const& t){
        return t;
      }
    }, *value);
  }
}

type_theory::raw::built_in::evaluation_result inductor(std::deque<type_theory::raw::term> const& vals){
  return type_theory::raw::built_in::require_values{{2}, [](std::deque<type_theory::raw::term> const& vals) -> type_theory::raw::built_in::evaluation_result{
    if(std::holds_alternative<type_theory::raw::built_in>(*vals[2])){
      return type_theory::raw::built_in::simplified{
        3,
        vals[1]
      };
    }else{
      auto app = std::get<type_theory::raw::application>(*vals[2]);
      return type_theory::raw::built_in::simplified{
        3,
        type_theory::raw::make_application(vals[0], app.x, type_theory::raw::make_application(type_theory::raw::make_builtin(&inductor, "induct"), vals[0], vals[1], app.x))
      };
    }
  }};
}
type_theory::raw::built_in::evaluation_result pair_inductor(std::deque<type_theory::raw::term> const& vals){
  return type_theory::raw::built_in::require_values{{1}, [](std::deque<type_theory::raw::term> const& vals) -> type_theory::raw::built_in::evaluation_result{
    auto app = std::get<type_theory::raw::application>(*vals[1]);
    auto app2 = std::get<type_theory::raw::application>(*app.f);
    return type_theory::raw::built_in::simplified{
      2,
      type_theory::raw::make_application(vals[0], app2.x, app.x)
    };
  }};
}
type_theory::raw::built_in::evaluation_result bool_inductor(std::deque<type_theory::raw::term> const& vals){
  return type_theory::raw::built_in::require_values{{2}, [](std::deque<type_theory::raw::term> const& vals) -> type_theory::raw::built_in::evaluation_result{
    bool v = std::get<type_theory::raw::built_in>(*vals[2]).name == "yes";
    return type_theory::raw::built_in::simplified{
      3,
      v ? vals[0] : vals[1]
    };
  }};
}

int main(){
  using namespace helpers;
  auto raw_type = raw::make_formal("type");
  expression type = get_type_t();
  expression boole = {raw_type, raw::make_formal("bool")};
  expression yes = {boole, raw::make_formal("yes")};
  expression no = {boole, raw::make_formal("no")};
  expression bool_induct = {func_type(
    named{"P",func_type(boole,type)},
    argument{"P"}(yes),
    argument{"P"}(no),
    named{"x", boole},
    argument{"P"}(argument{"x"})
  ), raw::make_lambda(raw::make_builtin(&bool_inductor, "bool_induct"), 0)};
  expression nat = {raw_type, raw::make_formal("nat")};
  expression zero = {nat, raw::make_formal("zero")};
  expression succ = {func_type(nat, nat), raw::make_formal("succ")};

  expression sum = {func_type(named{"a",type},func_type(argument{"a"},type),type), raw::make_formal("sum")};
  expression pair = {func_type(
    named{"a",type},
    named{"b",func_type(argument{"a"},type)},
    named{"x",argument{"a"}},
    argument{"b"}(argument{"x"}),
    sum(argument{"a"},argument{"b"})
  ), raw::make_lambda(raw::make_lambda(raw::make_formal("pair"),2),1)};
  expression pair_induct = {func_type(
    named{"a",type},
    named{"b",func_type(argument{"a"},type)},
    named{"P",func_type(sum(argument{"a"},argument{"b"}),type)},
    func_type(
      named{"x",argument{"a"}},
      named{"y",argument{"b"}(argument{"x"})},
      argument{"P"}(pair(argument{"a"},argument{"b"},argument{"x"},argument{"y"}))
    ),
    named{"p",sum(argument{"a"},argument{"b"})},
    argument{"P"}(argument{"p"})
  ), raw::make_lambda(raw::make_lambda(raw::make_lambda(raw::make_builtin(&pair_inductor,"pair_induct"),2),1),0)};


  auto encode = [&](std::size_t i){
    expression ret = zero;
    for(std::size_t j = 0; j < i; ++j)
      ret = succ(ret);
    return ret;
  };
  auto encode_var = [&](std::size_t i, expression t){
    expression ret = t;
    for(std::size_t j = 0; j < i; ++j)
      ret = succ(ret);
    return ret;
  };

  expression induct = {func_type(
    named{"P",func_type(nat,type)},
    func_type(named{"n",nat},func_type(argument{"P"}(argument{"n"}),argument{"P"}(succ(argument{"n"})))),
    argument{"P"}(zero),
    func_type(named{"n",nat},argument{"P"}(argument{"n"}))
  ), raw::make_lambda(raw::make_builtin(&inductor, "induct"), 0)};

  auto add = induct(
    lambda_expression{"", nat, func_type(nat, nat)},
    lambda_expression{"",nat,lambda_expression{"f",func_type(nat, nat), lambda_expression{"x",nat,succ(argument{"f"}(argument{"x"}))}}},
    lambda_expression{"x",nat, argument{"x"}}
  );
  auto cmp = induct(
    lambda_expression{"", nat, func_type(nat, boole)},
    lambda_expression{"",nat,lambda_expression{"f",func_type(nat, boole),
      induct(
        lambda_expression{"", nat, boole},
        lambda_expression{"n",nat,lambda_expression{"",boole,argument{"f"}(argument{"n"})}},
        no
      )
    }},
    induct(
      lambda_expression{"", nat, boole},
      lambda_expression{"n",nat,lambda_expression{"",boole,no}},
      yes
    )
  );
  expression mod_by = lambda_expression{"m", nat,
    induct(
      lambda_expression{"", nat, nat},
      lambda_expression{"", nat, lambda_expression{"n", nat,
        bool_induct(
          lambda_expression{"", boole, nat},
          zero,
          succ(argument{"n"}),
          cmp(argument{"m"}, succ(argument{"n"}))
        )
      }},
      zero
    )
  };
  expression mod = lambda_expression{"p", nat, lambda_expression{"q", nat, mod_by(argument{"q"},argument{"p"})}};

  std::cout << full_simplify(succ(succ(zero)).compile()) << "\n";

  std::cout << full_simplify(add(encode(5), encode(12))) << "\n";
  std::cout << full_simplify(cmp(encode(5), encode_var(5,{nat, raw::make_argument(1)}))) << "\n";

  std::cout << full_simplify(sum(nat, lambda_expression{"",nat,nat})) << "\n";

  auto nat_sum = sum(nat, lambda_expression{"",nat,nat});
  auto nat_pair = pair(nat, lambda_expression{"",nat,nat});
  auto nat_pair_induct = pair_induct(nat, lambda_expression{"",nat,nat});

  expression first_in_pair = full_simplify(nat_pair_induct(lambda_expression{"", nat_sum, nat}, lambda_expression{"x", nat, lambda_expression{"y", nat, argument{"x"}}}));
  expression zeroone = full_simplify(nat_pair(zero, encode(1)));

  expression next_fib = nat_pair_induct(lambda_expression{"", nat_sum, nat_sum}, lambda_expression{"x", nat, lambda_expression{"y", nat,
    nat_pair(add(argument{"x"}, argument{"y"}), argument{"x"})
  }});
  expression fib_n = lambda_expression{"n",nat,first_in_pair(induct(
    lambda_expression{"", nat, nat_sum},
    lambda_expression{"", nat, next_fib},
    zeroone,
    argument{"n"}
  ))};


  std::cout << full_simplify(fib_n(encode(6))) << "\n";

  std::cout << full_simplify(mod(fib_n(encode(7)), encode(5))) << "\n";
  return 0;
}
