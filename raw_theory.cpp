#include "raw_theory.hpp"

namespace type_theory{
  namespace raw{
    pair const& value::as_pair() const{ return std::get<pair>(data); }
    placeholder const& value::as_placeholder() const{ return std::get<placeholder>(data); }
    lazy_expression const& value::as_lazy_expression() const{ return std::get<lazy_expression>(data); }
    bool value::is_evaluated() const{ return !std::holds_alternative<lazy_expression>(data); }
    value_ptr make_pair(value_ptr first, value_ptr second){
      return std::make_shared<value>(pair{first, second});
    }
    value_ptr make_apply(value_ptr first, value_ptr second){
      return std::make_shared<value>(lazy_expression{first, second});
    }
    value_ptr make_placeholder(std::size_t index){
      return std::make_shared<value>(placeholder{index});
    }
    value_ptr make_empty(){
      static auto ret = std::make_shared<value>(empty{});
      return ret;
    }
    value_ptr make_id(){
      static auto ret = make_pair(make_function_head<function_head::id>(), make_empty());
      return ret;
    }
    value_ptr make_constant(value_ptr val){
      return make_pair(make_function_head<function_head::constant>(), val);
    }
    value_ptr make_native_pair(value_ptr first, value_ptr second){
      return make_pair(make_function_head<function_head::native>(), make_pair(first, second));
    }
    value_ptr make_apply_over(value_ptr first, value_ptr second){
      return make_pair(make_function_head<function_head::apply>(), make_pair(first, second));
    }
    value_ptr make_builtin(std::function<value_ptr(value_ptr)> f){
      return make_pair(make_function_head<function_head::builtin>(), make_primitive(builtin_definition(f)));
    }
    value_ptr apply(value_ptr f, value_ptr x){
      auto const& p = f->as_pair();
      switch(p.first->as_primitive<function_head>()){
        case function_head::constant:
          return p.second;
        case function_head::id:
          return x;
        case function_head::native:
        {
          auto const& inner = p.second->as_pair();
          return make_pair(apply(inner.first, x), apply(inner.second, x));
        }
        case function_head::apply:
        {
          auto const& inner = p.second->as_pair();
          return apply(apply(inner.first, x), apply(inner.second, x));
        }
        case function_head::builtin:
        {
          auto const& def = p.second->as_primitive<builtin_definition>();
          return def.func(x);
        }
      }
      std::terminate();
    }
    std::ostream& operator<<(std::ostream& o, empty){
      return o << "*";
    }
    std::ostream& operator<<(std::ostream& o, function_head f){
      switch(f){
        case function_head::constant: return o << "c";
        case function_head::id: return o << "id";
        case function_head::native: return o << "n";
        case function_head::apply: return o << "a";
        case function_head::builtin: return o << "b";
      }
      std::terminate();
    }
    std::ostream& operator<<(std::ostream& o, placeholder p){
      return o << "placeholder(" << p.index << ")";
    }
    std::ostream& operator<<(std::ostream& o, builtin_definition const&){
      return o << "(builtin)";
    }
    std::ostream& operator<<(std::ostream& o, pair const& p){
      return o << "(" << p.first << " , " << p.second << ")";
    }
    std::ostream& operator<<(std::ostream& o, lazy_expression const& p){
      return o << "(" << p.f << " " << p.x << ")";
    }
    std::ostream& operator<<(std::ostream& o, value const& v){
      std::visit([&](auto const& inner){ o << inner; }, v.data);
      return o;
    }
    std::ostream& operator<<(std::ostream& o, value_ptr const& v){
      return o << *v;
    }
  }
}
