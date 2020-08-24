#define typed_theory_h
#ifndef typed_theory_h

#include "raw_theory.hpp"
#include <exception>

namespace type_theory::typed{
  struct type_error : std::exception{
    const char* what() const noexcept override{ return "type error."; }
  };
  struct typed_value{
    raw::value_ptr type;
    raw::value_ptr value;
    typed_value get_type();
  };
  typed_value apply(typed_value f, typed_value x);


  /*
  enum class tristate_bool : unsigned char{
    no = 0, maybe = 1, yes = 2
  };
  tristate_bool operator&&(tristate_bool, tristate_bool);
  tristate_bool operator||(tristate_bool, tristate_bool);
  tristate_bool operator!(tristate_bool);
  */
};

#endif
