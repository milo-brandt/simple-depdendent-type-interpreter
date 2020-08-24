/*#include "typed_theory.hpp"

namespace type_theory::typed{

    /*
    using primitive_types = utility::class_list<empty, std::size_t, bool, type_head, built_in>;
    template<class T>
    concept primitive = primitive_types::contains<T>;

    struct pair{
      value_ptr first;
      value_ptr second;
    };
    struct application{
      value_ptr f;
      value_ptr x;
    };
    struct lambda{
      std::size_t arg_index;
      value_ptr body;
    };

    struct value{
      using any_types = primitive_types::append<pair, application, placeholder, lambda>;
    */
    /*bool match_types(raw::value_ptr a, raw::value_ptr b){
        /*std::visit(utility::overloaded{
            [&]<primitive T>(T const& a_data){
                if(!std::holds_alternative<T>(b->data)) throw type_error{};
                T const& b_data = std::get<T>(b);
                return a_data == b_data;
            }
        }, a->data);*/
        /*return true;
    }


    typed_value apply(typed_value f, typed_value x){
std::terminate();
    }
  /*
  tristate_bool operator&&(tristate_bool a, tristate_bool b){
    return (tristate_bool)std::min((unsigned char)(a), (unsigned char)(b));
  }
  tristate_bool operator||(tristate_bool a, tristate_bool b){
    return (tristate_bool)std::max((unsigned char)(a), (unsigned char)(b));
  }
  tristate_bool operator!(tristate_bool a){
    return (tristate_bool)(2 - (unsigned char)a);
  }


  //only called if types compare equal
  tristate_bool compare_functions(typed_value a, typed_value b);
  tristate_bool compare_types(raw::value_ptr a, raw::value_ptr b){

  }*/

//}
