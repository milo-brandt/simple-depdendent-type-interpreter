#include "raw_helper.hpp"
#include <unordered_map>
#include <cassert>

namespace type_theory{
  namespace raw{
    namespace helper{
      namespace{
        struct sub_data{
          std::size_t index;
          std::unordered_map<value_ptr, bool> is_constant;
          std::unordered_map<value_ptr, value_ptr> result;
        };
        bool is_constant(sub_data& data, value_ptr v);
        bool is_constant_impl(sub_data& data, value_ptr v){
          return std::visit(utility::overloaded{
            [&](pair const& p){ return is_constant(data, p.first) && is_constant(data, p.second); },
            [&](lazy_expression const& p){ return is_constant(data, p.f) && is_constant(data, p.x); },
            [&](placeholder const& p){ return p.index != data.index; },
            [&](auto const& primitive){ return true; }
          }, v->data);
        }
        bool is_constant(sub_data& data, value_ptr v){
          if(data.is_constant.contains(v)) return data.is_constant.at(v);
          else return data.is_constant[v] = is_constant_impl(data, v);
        }
        value_ptr convert(sub_data& data, value_ptr v);
        value_ptr convert_impl(sub_data& data, value_ptr v){
          if(is_constant(data, v)) return make_constant(v);
          return std::visit(utility::overloaded{
            [&](pair const& p){ return make_native_pair(convert(data, p.first), convert(data, p.second)); },
            [&](placeholder const& p){
              assert(p.index == data.index);
              return make_id();
            },
            [&](lazy_expression const& expr){ return make_apply_over(convert(data, expr.f), convert(data, expr.x)); },
            [&](auto const& primitive) -> value_ptr{ std::terminate(); }
          }, v->data);
        }
        value_ptr convert(sub_data& data, value_ptr v){
          if(data.result.contains(v)) return data.result.at(v);
          else return data.result[v] = convert_impl(data, v);
        }
      }
      value_ptr convert_placeholder_to_lambda(value_ptr val, std::size_t index){
        sub_data d{index};
        return convert(d, val);
      }
    }
  }
}
