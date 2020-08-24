#ifndef typed_theory_hpp
#define typed_theory_hpp

#include "raw_theory.hpp"

namespace type_theory{
  struct typed_term{
    raw::term type;
    raw::term value;
  };
  typed_term safe_apply(typed_term f, typed_term x, raw::name_scope const& names = {});
  template<class... Args>
  typed_term safe_apply(typed_term f, typed_term x, Args&&... args){
    return safe_apply(safe_apply(f, x), std::forward<Args>(args)...);
  }
  std::ostream& operator<<(std::ostream& o, typed_term const& t);
  typed_term full_simplify(typed_term in);

};

#endif
