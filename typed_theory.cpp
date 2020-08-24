#include "typed_theory.hpp"
#include <sstream>
#include <string>

namespace{
  template<class... Args>
  std::string convert_and_concatenate(Args&&... args){
    std::stringstream s;
    (s << ... << std::forward<Args>(args));
    return s.str();
  }
}

namespace type_theory{
  typed_term safe_apply(typed_term f, typed_term x, raw::name_scope const& names){
    auto ftype_base = try_simplify_at_top(f.type);
    if(!ftype_base.second)
      throw std::runtime_error(convert_and_concatenate("left hand side's type could not be normalized. type was ",full_simplify(f.type)));
    auto ftype = raw::extract_applications(ftype_base.first);
    if(ftype.second.size() != 2 || !std::holds_alternative<raw::built_in>(*ftype.first) || std::get<raw::built_in>(*ftype.first).name != "function")
      throw std::runtime_error(convert_and_concatenate("left hand side is not a function. type was ",full_simplify(f.type)," instead"));
    if(!raw::compare_literal(ftype.second[0],x.type,names))
      throw std::runtime_error(convert_and_concatenate("function expected type ",full_simplify(ftype.second[0])," got ",full_simplify(x.type)));
    return {make_application(ftype.second[1], x.value), make_application(f.value, x.value)};
  }
  std::ostream& operator<<(std::ostream& o, typed_term const& t){
    return o << make_application(t.value,raw::make_formal(":"),t.type);
  }
  typed_term full_simplify(typed_term in){
    return {full_simplify(in.type), full_simplify(in.value)};
  }
}
