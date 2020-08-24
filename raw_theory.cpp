/*#include "raw_theory.hpp"
#include <unordered_map>
#include <optional>
#include <stdlib.h> //for rand
#include "Templates/memoized_map.hpp"


namespace type_theory::raw{


  namespace{
    template<class callback>
    void for_each_reachable(value_ptr v, callback&& c){
      std::unordered_set<value_ptr, value_ptr::strict_hasher, value_ptr::strict_equality> seen;
      std::vector<value_ptr> stack;
      auto add_to_seen = [&](value_ptr v){
        if(seen.contains(v)) return;
        stack.push_back(v);
        seen.insert(v);
      };
      add_to_seen(v);
      while(!stack.empty()){
        value_ptr t = stack.back();
        stack.pop_back();
        std::visit(utility::overloaded{
          [&](auto const& p){ c(p); },
          [&](pair const& p){
            add_to_seen(p.first);
            add_to_seen(p.second);
            c(p);
          },
          [&](application const& e){
            add_to_seen(e.f);
            add_to_seen(e.x);
            c(e);
          },
          [&](lambda const& p){
            add_to_seen(p.body);
            c(p);
          },
          [&](placeholder const& p){
            c(p);
          }
        }, t->data);
      }
    }
    template<class callback>
    auto make_replacer(callback handler){
      return make_memoized_map<value_ptr, value_ptr, value_ptr::strict_hasher>([handler = std::move(handler)](auto& replace, value_ptr v) mutable{
        auto basic_visitor = utility::overloaded{
          [&](auto const&){ return v; },
          [&](pair const& p){
            auto n_first = replace(p.first);
            auto n_second = replace(p.second);
            if(n_first.get() == p.first.get() && n_second.get() == p.second.get()) return v;
            else return make_value(pair{n_first, n_second});
          },
          [&](application const& e){
            auto n_f = replace(e.f);
            auto n_x = replace(e.x);
            if(n_f.get() == e.f.get() && n_x.get() == e.x.get()) return v;
            else return make_value(application{n_f, n_x});
          },
          [&](placeholder const& p){ return v; },
          [&](lambda const& l){
            auto n_body = replace(l.body);
            if(n_body.get() == l.body.get()) return v;
            else return make_lambda(l.arg_index, n_body);
          }
        };
        auto full_visitor = [&](auto const& value) -> value_ptr{
          if constexpr(std::is_invocable_v<callback, decltype(replace), decltype(value)>){
            value_ptr ret = handler(replace, value);
            if(ret) return ret;
            else return v;
          }else{
            return basic_visitor(value);
          }
        };
        return std::visit(full_visitor, v->data);
      });
    }
    namespace{
      template<class index_mapper_t>
      struct untyped_scope_fixer_visitor{
        index_mapper_t mapper;
        value_ptr operator()(auto& replace, lambda const& l){
          auto replacement = mapper(l.arg_index);
          auto body_replacement = replace(l.body);
          if(body_replacement.get() == l.body.get() && replacement == l.arg_index) return nullptr;
          else return make_lambda(replacement, body_replacement);
        }
        value_ptr operator()(auto&, placeholder const& p){
          auto replacement = mapper(p.index);
          if(replacement == p.index) return nullptr;
          else return make_placeholder(replacement);
        }
      };
      template<class index_mapper_t>
      untyped_scope_fixer_visitor(index_mapper_t) -> untyped_scope_fixer_visitor<index_mapper_t>;
    };
    auto untyped_scope_fixer(untyped_scope s, untyped_scope inner){
      s.used_names.insert(0);
      for(auto name : s.used_names) inner.used_names.insert(name);
      auto index_mapper = make_memoized_map<std::size_t, std::size_t>([s = std::move(s), inner = std::move(inner)](auto&, std::size_t index) mutable -> std::size_t{
        if(s.used_names.contains(index)) return index; //index belongs to out of untyped_scope.
        if(!inner.used_names.contains(index)){
          inner.used_names.insert(index);
          return index;
        }
        while(true){
          std::size_t ret = rand();
          if(inner.used_names.contains(ret)) continue;
          inner.used_names.insert(ret);
          return ret;
        }
      });
      return untyped_scope_fixer_visitor(std::move(index_mapper));
    }
    auto replacing_handler(std::size_t index, value_ptr replacement){
      return [index, replacement](auto& replacer, placeholder const& p) -> value_ptr{
        if(p.index == index) return replacement;
        else return nullptr;
      };
    }
    value_ptr sanitize_argument(untyped_scope s, untyped_scope inner, value_ptr arg){
      return make_replacer(untyped_scope_fixer(std::move(s), std::move(inner)))(arg);
    }
    value_ptr perform_replacement(std::size_t index, value_ptr body, value_ptr sanitized_arg){
      return make_replacer(replacing_handler(index, sanitized_arg))(body);
    }
  }
  value_ptr apply_lambda(value_ptr f, value_ptr x, untyped_scope s){
    auto const& def = f->as_lambda();
    assert(def.arg_index);
    untyped_scope inner;
    for_each_reachable(def.body, utility::overloaded{
      [](auto const&){},
      [&](lambda const& l){
        inner.used_names.insert(l.arg_index);
      }
    });
    return perform_replacement(def.arg_index, def.body, sanitize_argument(std::move(s), std::move(inner), x));
  }
  namespace{
    std::pair<value_ptr, std::size_t> dive_through_evals(value_ptr start, std::size_t start_depth = 0){
      std::pair<value_ptr, std::size_t> ret(std::move(start), start_depth);
      while(ret.first->is_application()){
        ++ret.second;
        ret.first = ret.first->as_application().f;
      }
      return ret;
    }
    value_ptr dive_n_evals(value_ptr start, std::size_t dives){
      for(std::size_t i = 0; i < dives; ++i) start = start->as_application().f;
      return start;
    }
  }
  evaluation_result step_evaluation(value_ptr v, untyped_scope s){
    return std::visit(utility::overloaded{
      [&](auto const&) -> evaluation_result{ return evaluation_result::nothing_to_do; },
      [&](pair const& p) -> evaluation_result{ return evaluation_result::nothing_to_do; },
      [&](application const& e) -> evaluation_result{
        std::size_t arg_ct = 1;
        auto [inner, depth] = dive_through_evals(e.f, 1);
        if(inner->is_placeholder()) return evaluation_result::need_values({inner});
        assert(inner->is_lambda());
        auto sub_index = inner->as_lambda().arg_index;
        if(sub_index){
          if(depth == 1){
            return evaluation_result::simplified(apply_lambda(e.f, e.x, std::move(s)));
          }else{
            return evaluation_result::need_values({dive_n_evals(e.f, depth - 2)});
          }
        }else{
          assert(inner->as_lambda().body->is_primitive<built_in>());
          auto const& fn_def = inner->as_lambda().body->as_primitive<built_in>();
          if(depth < fn_def.args_required) return evaluation_result::nothing_to_do;
          else if(depth == fn_def.args_required){
            std::vector<value_ptr> v;
            v.reserve(depth);
            v.push_back(e.x);
            value_ptr pos = e.f;
            for(int i = 1; i < depth; ++i){
              auto const& l = pos->as_application();
              v.push_back(l.x);
              pos = l.f;
            }
            auto ret = fn_def.func(std::move(v));
            if(std::holds_alternative<evaluation_result::simplified>(ret.status)){
              auto& simplification = std::get<evaluation_result::simplified>(ret.status);
              simplification.value = sanitize_argument({}, std::move(s), simplification.value);
            }
            return ret;
          }else{
            return evaluation_result::need_values({dive_n_evals(e.f, depth - 1 - fn_def.args_required)});
          }
        }
      },
      [&](placeholder const& p) -> evaluation_result{ return evaluation_result::value_not_available; }
    }, v->data);
  }
  value_ptr top_eval(value_ptr input, untyped_scope s){
    while(true){
      auto result = step_evaluation(input, s);
      if(std::holds_alternative<evaluation_result::nothing_to_do_t>(result.status)) return input;
      if(std::holds_alternative<evaluation_result::value_not_available_t>(result.status)) throw "Cannot evaluate.";
      if(std::holds_alternative<evaluation_result::need_values>(result.status)){
        for(auto const& val : std::get<evaluation_result::need_values>(result.status).desired){
          top_eval(val, s);
        }
      }
      if(std::holds_alternative<evaluation_result::simplified>(result.status)){
        input.forward_to(std::get<evaluation_result::simplified>(result.status).value);
      }
    }
  }
  value_ptr deep_eval(value_ptr input, untyped_scope s){
    try{
      top_eval(input, s);
    }catch(...){}
    std::visit(utility::overloaded{
      [](primitive auto const&){},
      [](placeholder const&){},
      [&](application const& e){
        deep_eval(e.f, s);
        deep_eval(e.x, s);
      },
      [&](pair const& p){
        deep_eval(p.first, s);
        deep_eval(p.second, s);
      },
      [&](lambda const& l){
        s.used_names.insert(l.arg_index);
        deep_eval(l.body, s);
      },
    }, input->data);
    return input;
  }
  std::ostream& operator<<(std::ostream& o, built_in const& b){
    return o << b.name;
  }
  std::ostream& operator<<(std::ostream& o, empty){
    return o << "*";
  }
  std::ostream& operator<<(std::ostream& o, type_head h){
    /*switch(h){
      case type_head::c_type: o << "type";
      case type_head::c_empty: o << "empty";
      case type_head::c_size_t: o << "size_t";
      case type_head::function: o << "function";
  }*//*
  o << "type head";
    std::terminate();
  }
  std::ostream& operator<<(std::ostream& o, value_ptr v){
    std::visit(utility::overloaded{
      [&](auto const& v){ o << v; },
      [&](lambda const& l){
        if(l.arg_index){
          o << "(\\" << l.arg_index << " -> " << l.body << ")";
        }else{
          o << l.body->as_primitive<built_in>();
        }
      },
      [&](pair const& p){
        o << "(" << p.first << " , " << p.second << ")";
      },
      [&](application const& e){
        o << "(" << e.f << " " << e.x << ")";
      },
      [&](placeholder const& p){
        o << "$" << p.index;
      }
    }, v->data);
    return o;
  }
}*/
