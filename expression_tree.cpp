#include "expression_tree.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include "Templates/TemplateUtility.h"

inline std::size_t hash_combine(std::size_t seed, std::size_t added)
{
    return seed ^ (added + 0x9e3779b9 + (seed<<6) + (seed>>2));
}
template<class index, class arg, class hasher, class maker>
arg const& get_or_insert(std::unordered_map<index, arg, hasher>& map, index const& i, maker&& f) {
  if(map.find(i) != map.end()) {
    return map[i];
  } else {
    return map.insert(std::make_pair(i, f())).first->second;
  }
}
/*
primitive stuff
*/
namespace type_theory{
  namespace{
    struct primitive_reduced{
      std::size_t args_used;
      expression ret;
    };
    struct primitive_canonical_t{};
    constexpr primitive_canonical_t primitive_canonical;
    struct primitive_failed_t{};
    constexpr primitive_failed_t primitive_failed;
    using primitive_result = std::variant<primitive_reduced, primitive_canonical_t, primitive_failed_t>;
    primitive_result match_to_pattern_function(expression const& me, pattern_function_def const& def, std::vector<expression> const& rev_args);
    primitive_result apply_to_primitive(primitive const& p, std::vector<expression> const& rev_args) {
      return std::visit(utility::overloaded{
        [&](primitives::external_function const& f) -> primitive_result{
          if(f.args_required > rev_args.size()){
            return primitive_canonical;
          }else{
            std::vector<primitives::basic> args;
            args.reserve(f.args_required);
            for(std::size_t i = 0; i < f.args_required; ++i) {
              auto expr = left_canonical_form(rev_args[rev_args.size() - 1 - i]);
              if(!expr.second){
                return primitive_failed;
              }
              auto node = expr.first.read();
              if(!std::holds_alternative<primitive>(node)){
                return primitive_failed;
              }
              auto pval = std::get<primitive>(node).value;
              if(!std::holds_alternative<primitives::basic>(pval)){
                return primitive_failed;
              }
              args.push_back(std::get<primitives::basic>(pval));
            }
            return primitive_reduced{f.args_required, f.f(args)};
          }
        },
        [&](primitives::pattern_function const& f){ return match_to_pattern_function(primitive{f}, *f.def, rev_args); },
        [&](primitives::exact_comparison_function_t const&) -> primitive_result{
          if(rev_args.size() < 2) return primitive_canonical;
          auto expr_lhs = full_canonical_form(rev_args[rev_args.size() - 1]);
          if(!expr_lhs.is_constant_below(0)) return primitive_failed;
          auto expr_rhs = full_canonical_form(rev_args[rev_args.size() - 2]);
          if(!expr_rhs.is_constant_below(0)) return primitive_failed;
          return primitive_reduced{2, primitive{primitives::basic(expr_lhs == expr_rhs)}};
        },
        [](auto const&) -> primitive_result{ return primitive_canonical; }
      }, p.value);
    }
    void dive_left(expression& head, std::vector<expression>& args);
    enum class pattern_match_result{
      ok,
      not_matched,
      failed
    };
    pattern_match_result match_to_expression(pattern_function_pat const& pat, expression e, std::vector<expression>& captures){
      auto [reduced, canonical] = left_canonical_form(e);
      if(!canonical) return pattern_match_result::failed;
      std::vector<expression> args;
      dive_left(reduced, args);
      if(args.size() != pat.sub_patterns.size()) return pattern_match_result::not_matched;
      auto head = reduced.read();
      if(!std::holds_alternative<primitive>(head) || std::get<primitive>(head) != pat.head) return pattern_match_result::not_matched;
      for(std::size_t matching_index = 0; matching_index < pat.sub_patterns.size(); ++matching_index){
        auto const& p = pat.sub_patterns[matching_index];
        auto const& e = args[args.size() - 1 - matching_index];
        if(p){
          auto subresult = match_to_expression(*p, e, captures);
          if(subresult != pattern_match_result::ok) return subresult; //theoretically, could be improved by trying to turn "failed" into "not_matched"
        }else{
          captures.push_back(e);
        }
      }
      return pattern_match_result::ok;
    }
    enum class case_failure_type{
      canonical,
      not_applicable,
      failed
    };
    std::variant<case_failure_type, expression> try_to_apply_case(expression const& me, pattern_function_case const& f, std::vector<expression> const& rev_args){
      if(f.pattern.size() > rev_args.size()) return case_failure_type::canonical;
      std::vector<expression> captures;
      captures.push_back(me);
      for(std::size_t matching_index = 0; matching_index < f.pattern.size(); ++matching_index){
        auto const& expr = rev_args[rev_args.size() - 1 - matching_index];
        if(f.pattern[matching_index] == nullptr){
          captures.push_back(expr);
          continue;
        }
        auto const& pat = *f.pattern[matching_index];
        auto result = match_to_expression(pat, expr, captures);
        if(result == pattern_match_result::not_matched) return case_failure_type::not_applicable;
        if(result == pattern_match_result::failed) return case_failure_type::failed;
      }
      return substitute_all_args(f.replacement, captures);
    }
    primitive_result match_to_pattern_function(expression const& me, pattern_function_def const& def, std::vector<expression> const& rev_args){
      for(auto const& cas: def.cases){
        auto res = try_to_apply_case(me, cas, rev_args);
        if(res.index() == 1){
          return primitive_reduced{cas.pattern.size(), std::get<1>(res)};
        }else{
          auto res_t = std::get<0>(res);
          if(res_t == case_failure_type::canonical) return primitive_canonical;
          if(res_t == case_failure_type::failed) return primitive_failed;
        }
      }
      return primitive_failed;
    }
    void output_primitive(std::ostream& o, primitive const& p) {
      std::visit(utility::overloaded{
        [&](primitives::basic const& b){
          std::visit([&](auto const& v){ o << v; }, b);
        },
        [&](primitives::exact_comparison_function_t const& f){
          o << "===";
        },
        [&](primitives::external_function const& f){
          o << f.name;
        },
        [&](primitives::marker mark){
          switch(mark){
            case primitives::marker::type: o << "type"; return;
            case primitives::marker::function: o << "function"; return;
          }
        },
        [&](primitives::constructor const& c){
          o << c.name;
        },
        [&](primitives::pattern_function const& pat){
          o << pat.def->name;
        }
      }, p.value);
    }
  }
}
/*
tree stuff
*/

namespace type_theory {
  struct expression::detail { //evil static internment stuff
    struct pair_hasher{
      std::hash<std::size_t> h;
      std::size_t operator()(std::pair<std::size_t, std::size_t> const& p) const noexcept {
        return hash_combine(h(p.first), h(p.second));
      }
    };
    struct primitive_hasher{
      std::size_t operator()(primitive const& p) const noexcept {
        return hash_combine(p.value.index(), std::visit(utility::overloaded{
          [&](primitives::basic const& b){
            return std::visit([&]<class T>(T const& v){ return std::hash<T>{}(v); }, b);
          },
          [&](primitives::external_function const& f){ return std::hash<decltype(f.f)>{}(f.f); },
          [&](primitives::marker mark) -> std::size_t{
            switch(mark){
              case primitives::marker::type: return 0;
              case primitives::marker::function: return 1;
            }
            std::terminate();
          },
          [&](primitives::constructor const& c){ return std::hash<std::string>{}(c.name); },
          [&](primitives::pattern_function const& pat){ return std::hash<std::shared_ptr<pattern_function_def> >{}(pat.def); },
          [&](primitives::exact_comparison_function_t const&){ return (std::size_t)0; }
        }, p.value));
      }
    };
    struct lambda_entry{
      expression body;
      std::string arg_name;
    };
    struct argument_entry{
      std::size_t stack_index;
    };
    struct application_entry{
      expression f;
      expression x;
    };
    struct primitive_entry{
      primitive value;
    };
    struct entry {
      std::variant<lambda_entry, argument_entry, application_entry, primitive_entry> entry;
      std::size_t ref_ct;
      std::optional<expression> best_simplification;
    };
    using entry_or_free_list = std::variant<entry, std::optional<std::size_t> >;
    struct context {
      std::optional<std::size_t> free_pos;
      std::vector<entry_or_free_list> entries;
      std::unordered_map<primitive, std::size_t, primitive_hasher> interned_primitives;
      std::unordered_map<std::size_t, std::size_t> interned_arguments;
      std::unordered_map<std::size_t, std::size_t> interned_lambdas;
      std::unordered_map<std::pair<std::size_t, std::size_t>, std::size_t, pair_hasher> interned_applications;
      std::size_t add_entry(entry e) {
        if(free_pos) {
          std::size_t ret = *free_pos;
          free_pos = std::get<1>(entries[ret]);
          entries[ret] = std::move(e);
          return ret;
        } else {
          std::size_t ret = entries.size();
          entries.push_back(std::move(e));
          return ret;
        }
      }
      std::size_t ref_entry(std::size_t index) {
        ++std::get<0>(entries[index]).ref_ct;
        return index;
      }
      void deref_entry(std::size_t index) {
        entry_or_free_list& full_e = entries[index];
        entry& e = std::get<0>(full_e);
        if(--e.ref_ct == 0) {
          std::visit(utility::overloaded{
            [this](primitive_entry const& p){ interned_primitives.erase(p.value); },
            [this](argument_entry const& a){ interned_arguments.erase(a.stack_index); },
            [this](lambda_entry const& l){ interned_lambdas.erase(*l.body.index); },
            [this](application_entry const& a){ interned_applications.erase(std::make_pair(*a.f.index, *a.x.index)); }
          }, e.entry);
          full_e = free_pos;
          free_pos = index;
        }
      }
      void add_simpler_form(expression const& a, expression e) {
        if(*e.index != *a.index){
          std::get<0>(entries[*a.index]).best_simplification = std::move(e);
        }
      }
      /*~context(){
        for(std::size_t index = 0; index < entries.size(); ++index) {
          if(entries[index].index() == 0) {
            auto const& i = std::get<0>(entries[index]);
            std::cout << "index: " << index;
            if(i.best_simplification){
              std::cout << " with simplificiation " << *i.best_simplification->index;
            }
            std::cout << "\n";
          }
        }
      }*/
    };
    static context& get_context(){
      static context ret;
      return ret;
    }
  };
  expression::expression() noexcept = default;
  expression::expression(expression const& a) noexcept: index(a.index) {
    if(index){
      detail::get_context().ref_entry(*index);
    }
  }
  expression::expression(expression&& a) noexcept: index(a.index) {
    a.index = std::nullopt;
  }
  expression::expression(application a) noexcept:index(get_or_insert(
    detail::get_context().interned_applications,
    std::make_pair(*a.f.index, *a.x.index),
    [&a](){ return detail::get_context().add_entry({ detail::application_entry{ std::move(a.f), std::move(a.x) } }); }
  )){
    detail::get_context().ref_entry(*index);
  }
  expression::expression(lambda l) noexcept:index(get_or_insert(
    detail::get_context().interned_lambdas,
    *l.body.index,
    [&l](){ return detail::get_context().add_entry({ detail::lambda_entry{ std::move(l.body), std::move(l.arg_name) } }); }
  )){
    detail::get_context().ref_entry(*index);
  }
  expression::expression(argument a) noexcept:index(get_or_insert(
    detail::get_context().interned_arguments,
    a.stack_index,
    [&a](){ return detail::get_context().add_entry({ detail::argument_entry{ a.stack_index } }); }
  )){
    detail::get_context().ref_entry(*index);
  }
  expression::expression(primitive p) noexcept:index(get_or_insert(
    detail::get_context().interned_primitives,
    p,
    [&p](){ return detail::get_context().add_entry({ detail::primitive_entry{ p } }); }
  )){
    detail::get_context().ref_entry(*index);
  }
  expression::~expression() noexcept{
    if(index){
      detail::get_context().deref_entry(*index);
    }
  }
  expression& expression::operator=(expression const& other) noexcept {
    expression tmp = other;
    this->~expression();
    new (this) expression(std::move(tmp));
    return *this;
  }
  expression& expression::operator=(expression&& other) noexcept {
    std::swap(index, other.index);
    return *this;
  }
  bool expression::has_value() const {
    return index.has_value();
  }
  expression::operator bool() const{
    return has_value();
  }
  expression expression::get_simplest_existing_form() const{
    auto& best = std::get<0>(detail::get_context().entries[*index]).best_simplification;
    if(best){
      return *(best = best->get_simplest_existing_form());
    } else {
      return *this;
    }
  }
  node expression::read() const {
    return std::visit(utility::overloaded{
      [](detail::application_entry const& e) -> node { return application{ e.f, e.x }; },
      [](detail::lambda_entry const& e) -> node { return lambda{ e.arg_name, e.body }; },
      [](detail::argument_entry const& e) -> node { return argument{e.stack_index}; },
      [](detail::primitive_entry const& e) -> node { return e.value; }
    }, std::get<0>(detail::get_context().entries[*index]).entry);
  }
  bool expression::operator==(expression const& c) const{
    return index == c.index;
  }
  expression expression::operator()(expression x) const{
    return application{*this, std::move(x)};
  }
  bool expression::is_constant_in(std::size_t index) const{
    return std::visit(utility::overloaded{
      [&](primitive const& p){ return true; },
      [&](application const& a) { return a.f.is_constant_in(index) && a.x.is_constant_in(index); },
      [&](lambda const& l) { return l.body.is_constant_in(index + 1); },
      [&](argument const& a) { return a.stack_index != index; }
    }, read());
  }
  bool expression::is_constant_below(std::size_t index) const{
    return std::visit(utility::overloaded{
      [&](primitive const& p){ return true; },
      [&](application const& a) { return a.f.is_constant_below(index) && a.x.is_constant_below(index); },
      [&](lambda const& l) { return l.body.is_constant_below(index + 1); },
      [&](argument const& a) { return a.stack_index < index; }
    }, read());
  }
  namespace{
    expression deepen_impl(expression expr, std::size_t added_depth, std::size_t bound_depth) {
      return std::visit(utility::overloaded{
        [&](primitive const& p){ return std::move(expr); },
        [&](application const& a) -> expression { return application{deepen_impl(std::move(a.f), added_depth, bound_depth), deepen_impl(std::move(a.x), added_depth, bound_depth)}; },
        [&](lambda const& l) -> expression { return lambda{std::move(l.arg_name), deepen_impl(std::move(l.body), added_depth, bound_depth + 1)}; },
        [&](argument const& a) -> expression { if(a.stack_index < bound_depth){ return std::move(expr); } else { return argument{a.stack_index + added_depth}; } }
      }, expr.read());
    }
    expression deepen(expression expr, std::size_t added_depth) {
      if(added_depth == 0){
        return expr;
      } else {
        return deepen_impl(std::move(expr), added_depth, 0);
      }
    }
    expression beta_substitution_impl(expression lambda_body, expression const& arg, std::size_t depth){
      return std::visit(utility::overloaded{
        [&](primitive const& p){ return std::move(lambda_body); },
        [&](application const& a) -> expression { return application{beta_substitution_impl(std::move(a.f), arg, depth), beta_substitution_impl(std::move(a.x), arg, depth)}; },
        [&](lambda const& l) -> expression { return lambda{std::move(l.arg_name), beta_substitution_impl(std::move(l.body), arg, depth + 1)}; },
        [&](argument const& a) -> expression {
          if(a.stack_index == depth){
            return deepen(arg, depth);
          } else if(a.stack_index < depth){
            return std::move(lambda_body); //it's bound
          } else {
            return argument{a.stack_index - 1}; //it's outside and deeper than bound arg
          }
        }
      }, lambda_body.read());
    }
  }
  expression beta_substitution(expression lambda_body, expression const& arg){
    return beta_substitution_impl(std::move(lambda_body), arg, 0);
  }
  namespace{
    expression substitute_all_args_impl(expression lambda_body, std::vector<expression> const& args, std::size_t depth){
      return std::visit(utility::overloaded{
        [&](primitive const& p){ return std::move(lambda_body); },
        [&](application const& a) -> expression { return application{substitute_all_args_impl(std::move(a.f), args, depth), substitute_all_args_impl(std::move(a.x), args, depth)}; },
        [&](lambda const& l) -> expression { return lambda{std::move(l.arg_name), substitute_all_args_impl(std::move(l.body), args, depth + 1)}; },
        [&](argument const& a) -> expression {
          if(a.stack_index < depth){
            return std::move(lambda_body); //it's bound
          } else {
            assert(a.stack_index - depth < args.size());
            return deepen(args[a.stack_index - depth], depth);
          }
        }
      }, lambda_body.read());
    }
  }
  expression substitute_all_args(expression lambda_body, std::vector<expression> const& args){
    return substitute_all_args_impl(std::move(lambda_body), args, 0);
  }
  namespace{
    void dive_left(expression& head, std::vector<expression>& args) {
      while(head = head.get_simplest_existing_form(), std::visit(utility::overloaded{ //oh you thought C++ was pretty? lmao
        [&](application const& a){
          head = std::move(a.f);
          args.push_back(std::move(a.x));
          return true;
        },
        [](auto const&){ return false; }
      }, head.read()));
    }
    enum class finishing_result{
      done,
      not_done,
      failed
    };
  }
  std::pair<expression, bool> left_canonical_form(expression head){
    struct detail{
      static void pop_n_args_and_simplify(expression& head, std::vector<expression>& rev_args, expression new_head, std::size_t popped) {
        assert(popped <= rev_args.size());
        for(std::size_t i = 0; i < popped; ++i) {
          head = application{std::move(head), std::move(rev_args.back())};
          rev_args.pop_back();
        }
        expression::detail::get_context().add_simpler_form(head, new_head);
        head = std::move(new_head);
      }
    };
    std::vector<expression> args;
    while(true){
      dive_left(head, args);
      auto done = std::visit(utility::overloaded{
        [&](primitive const& p){
          auto result = apply_to_primitive(p, args);
          return std::visit(utility::overloaded{
            [](primitive_failed_t){ return finishing_result::failed; },
            [](primitive_canonical_t){ return finishing_result::done; },
            [&](primitive_reduced& r){
              detail::pop_n_args_and_simplify(head, args, r.ret, r.args_used);
              //args.erase(args.end() - r.args_used, args.end());
              //head = r.ret;
              return finishing_result::not_done;
            }
          }, result);
        },
        [&](application const& a) -> finishing_result { std::terminate(); }, //unreachable because of dive_left
        [&](lambda const& l){
          if(args.size() > 0){
            detail::pop_n_args_and_simplify(head, args, beta_substitution(std::move(l.body), args.back()), 1);
            return finishing_result::not_done;
          } else {
            return finishing_result::done;
          }
        },
        [&](argument const& a){
          return finishing_result::failed;
        }
      }, head.read());
      if(done != finishing_result::not_done){
        for(auto i = args.rbegin(); i != args.rend(); ++i){
          head = application{std::move(head), std::move(*i)};
        }
        return std::make_pair(head, done == finishing_result::done);
      }
    }
  }
  expression full_canonical_form(expression expr) {
    expression head = left_canonical_form(std::move(expr)).first;
    expression ret = std::visit(utility::overloaded{
      [&](primitive const& p){ return std::move(head); },
      [&](application const& a) -> expression { return application{full_canonical_form(std::move(a.f)), full_canonical_form(std::move(a.x))}; }, //unreachable because of dive_left
      [&](lambda const& l) -> expression { return lambda{std::move(l.arg_name), full_canonical_form(l.body)}; },
      [&](argument const& a){ return std::move(head); }
    }, head.read());
    expression::detail::get_context().add_simpler_form(expr, ret);
    return ret;
  }
  namespace{
    struct print_name_data{
      std::vector<std::string> var_names;
      std::unordered_set<std::string> used_names;
      template<class F>
      auto call_with_anonymous_var(F&& f){
        var_names.push_back("_");
        auto ret = std::forward<F>(f)(*this);
        var_names.pop_back();
        return ret;
      }
      template<class F>
      auto call_with_new_var(std::string name, F&& f){
        while(used_names.find(name) != used_names.end()) name += "'";
        used_names.insert(name);
        var_names.push_back(name);
        auto ret = std::forward<F>(f)(*this);
        var_names.pop_back();
        used_names.erase(name);
        return ret;
      }
      std::string get_name(std::size_t index){
        if(index > var_names.size()){
          std::stringstream s;
          s << "$" << index;
          return s.str();
        }else{
          return var_names[var_names.size() - 1 - index];
        }
      }
    };
    enum class expr_binop: unsigned char{
      application,
      inhabits,
      function_arrow,
      lambda_arrow
    };
    std::ostream& operator<<(std::ostream& o, expr_binop op){
      switch(op){
        case expr_binop::application: return o << " ";
        case expr_binop::inhabits: return o << ": ";
        case expr_binop::function_arrow: return o << " -> ";
        case expr_binop::lambda_arrow: return o << " => ";
      }
      std::terminate();
    }
    enum class associativity_direction{
      left,
      right
    };
    struct binop_tree{
      std::variant<std::string, std::tuple<expr_binop, std::unique_ptr<binop_tree>, std::unique_ptr<binop_tree> > > data;
      template<class Arg>
      binop_tree(Arg&& a):data(std::forward<Arg>(a)){}
    };
    bool need_parenthesis(expr_binop outer, associativity_direction side, expr_binop inner) {
      if((unsigned char)inner < (unsigned char)outer) return false;
      if(outer == inner){
        switch(outer){
          case expr_binop::application: return side == associativity_direction::right;
          case expr_binop::inhabits: return side == associativity_direction::left;
          case expr_binop::function_arrow: return side == associativity_direction::left;
          case expr_binop::lambda_arrow: return side == associativity_direction::left;
        }
      }
      return true;
    }
    void print_binop_tree(std::ostream& o, binop_tree const& t, expr_binop outer, associativity_direction side) {
      std::visit(utility::overloaded{
        [&](std::string const& s){ o << s; },
        [&](std::tuple<expr_binop, std::unique_ptr<binop_tree>, std::unique_ptr<binop_tree> > const& tup){
          bool need_parens = need_parenthesis(outer, side, std::get<0>(tup));
          if(need_parens) o << "(";
          print_binop_tree(o, *std::get<1>(tup), std::get<0>(tup), associativity_direction::left);
          o << std::get<0>(tup);
          print_binop_tree(o, *std::get<2>(tup), std::get<0>(tup), associativity_direction::right);
          if(need_parens) o << ")";
        }
      },t.data);
    }
    std::unique_ptr<binop_tree> make_binop_tree(expression const& expr, print_name_data& p){
      return std::visit(utility::overloaded{
        [&](primitive const& p){ std::stringstream s; output_primitive(s, p); return std::make_unique<binop_tree>(s.str()); },
        [&](application const& a){ return std::make_unique<binop_tree>(std::make_tuple(expr_binop::application, make_binop_tree(a.f, p), make_binop_tree(a.x, p))); },
        [&](lambda l){
          std::string arg_name = l.arg_name.empty() ? "x" : l.arg_name;
          if(l.body.is_constant_in(0)){
            return std::make_unique<binop_tree>(std::make_tuple(expr_binop::lambda_arrow, std::make_unique<binop_tree>("_"), p.call_with_anonymous_var([&](print_name_data& p){ return make_binop_tree(l.body, p); })));
          }else{
            return std::make_unique<binop_tree>(std::make_tuple(expr_binop::lambda_arrow, std::make_unique<binop_tree>(arg_name), p.call_with_new_var(arg_name, [&](print_name_data& p){ return make_binop_tree(l.body, p); })));
          }
        },
        [&](argument const& a){ return std::make_unique<binop_tree>(p.get_name(a.stack_index)); }
      }, expr.read());
    }
  }
  std::ostream& operator<<(std::ostream& o, expression const& expr) {
    /*std::visit(utility::overloaded{
      [&o](primitive const& p){ output_primitive(o, p); },
      [&o](application const& a){ o << "(" << a.f << " " << a.x << ")"; },
      [&o](lambda const& l){ o << "lambda(" << l.body << ")"; },
      [&o](argument const& a){ o << "arg(" << a.stack_index << ")"; }
    }, expr.read());
    return o;*/
    print_name_data p;
    print_binop_tree(o, *make_binop_tree(expr, p), expr_binop::lambda_arrow, associativity_direction::right);
    return o;
  }

}
