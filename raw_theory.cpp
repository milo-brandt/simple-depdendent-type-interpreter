#include "raw_theory.hpp"

namespace type_theory::raw{
  term make_lambda(term body, std::size_t arg_index){
      return make_term(lambda_base{body, arg_index});
  }
  term make_argument(std::size_t arg_index){
      return make_term(argument{arg_index});
  }
  term make_primitive(utility::printable_any value){
      return make_term(primitive{std::move(value)});
  }
  term make_application(term f, term x){
      return make_term(application{f, x});
  }
  term make_application(term base, std::deque<term> const& arguments){
      for(term arg : arguments){
          base = make_application(base, arg);
      }
      return base;
  }
  term make_builtin(built_in::evaluator f, std::string name){
      return make_term(built_in{std::move(f), std::move(name)});
  }
  term make_formal(std::string name){
    return make_builtin([](auto const&){ return built_in::nothing_to_do; }, std::move(name));
  }

  bool name_scope::contains(std::size_t name) const{ return used_names.contains(name); }
  void name_scope::add_name(std::size_t name){ used_names.insert(name); }
  void name_scope::erase_name(std::size_t name){ used_names.erase(name); }
  void name_scope::ensure_name(std::size_t name){ if(!contains(name)) add_name(name); }

  namespace{
      void collect_dependent_names_of(term base, name_scope& v){
          std::unordered_set<term> terms_examined;
          std::vector<term> terms_waiting;
          auto enqueue_term = [&terms_examined,&terms_waiting](term const& x){
              if(!terms_examined.contains(x)){
                  terms_examined.insert(x);
                  terms_waiting.push_back(x);
              }
          };
          enqueue_term(base);
          while(!terms_waiting.empty()){
              term current = terms_waiting.back();
              terms_waiting.pop_back();
              std::visit(utility::overloaded{
                  [&](lambda_base const& l){
                      enqueue_term(l.body);
                      v.ensure_name(l.arg_index);
                  },
                  [&](application const& a){
                      enqueue_term(a.f);
                      enqueue_term(a.x);
                  },
                  [&](auto const&){}
              }, *current);
          }
      }
      struct renaming_data{
          name_scope used_names;
          std::unordered_map<std::size_t, std::size_t> renamings;
          std::size_t add_name(std::size_t original, std::size_t transformed){
              used_names.add_name(transformed);
              renamings.insert(std::make_pair(original, transformed));
              return transformed;
          }
          std::size_t add_name(std::size_t original){
              std::size_t candidate = original;
              while(true){
                  if(!used_names.contains(candidate)) return add_name(original, candidate);
                  candidate = rand();
              }
          }
          std::size_t rename_argument(std::size_t c){
              if(renamings.contains(c)) return renamings[c];
              else return add_name(c, c); //free variable - encountered before lambda abstraction
          }
          std::size_t rename_lambda(std::size_t c){
              if(renamings.contains(c)) return renamings[c];
              else return add_name(c);
          }
      };
      struct sanitizing_data{
          renaming_data renamings;
          std::unordered_map<term, term> computed_values;
          term sanitize(term input){
              if(computed_values.contains(input)) return computed_values[input];
              return computed_values[input] = std::visit<term>(utility::overloaded{
                  [&](lambda_base const& l){
                      auto new_index = renamings.rename_lambda(l.arg_index);
                      return make_term(lambda_base{sanitize(l.body), new_index});
                  },
                  [&](application const& a){
                      return make_term(application{sanitize(a.f), sanitize(a.x)});
                  },
                  [&](argument const& a){
                      return make_term(argument{renamings.rename_argument(a.arg_index)});
                  },
                  [&](auto const&){
                      return input;
                  }
              }, *input);
          }
      };
      struct substituting_data{
          std::size_t index;
          term replacement;
          std::unordered_map<term, term> computed_values;
          term substitute(term input){
              if(computed_values.contains(input)) return computed_values[input];
              return computed_values[input] = std::visit<term>(utility::overloaded{
                  [&](lambda_base const& l){
                      return make_term(lambda_base{substitute(l.body), l.arg_index});
                  },
                  [&](application const& a){
                      return make_term(application{substitute(a.f), substitute(a.x)});
                  },
                  [&](argument const& a){
                      return a.arg_index == index ? replacement : input;
                  },
                  [&](auto const&){
                      return input;
                  }
              }, *input);
          }
      };
      term literal_substitution(term base, std::size_t index, term replacement){
          return substituting_data{
              .index = index,
              .replacement = replacement
          }.substitute(base);
      }
  }

  std::pair<term, std::deque<term> > extract_applications(term t){
    term pos = t;
    std::deque<term> args;
    while(std::holds_alternative<application>(*pos)){
      auto const& a = std::get<application>(*pos);
      args.push_front(a.x);
      pos = a.f;
    }
    return {pos, std::move(args)};
  }

  //base and arg are both individually valid, and no free term in either is bound in the other.
  term sanitize_term(term base, name_scope used_names){
      return sanitizing_data{
          .renamings = {
              .used_names = std::move(used_names)
          }
      }.sanitize(base);
  }
  term apply_lambda(lambda_base const& f, term arg, name_scope const& s){
      name_scope used = s;
      collect_dependent_names_of(f.body, used);
      return literal_substitution(f.body, f.arg_index, sanitize_term(arg, std::move(used)));
  }
  std::pair<term, bool> try_simplify_at_top(term t, name_scope const& scope){
      std::deque<term> arguments;
      term pos = t;
      //Invariant: t equals the application of arguments to pos.
      while(true){
          while(std::holds_alternative<application>(*pos)){
              application const& a = std::get<application>(*pos);
              arguments.push_front(a.x);
              pos = a.f;
          }
          if(arguments.empty()) return {pos, !std::holds_alternative<argument>(*pos)};
          if(std::holds_alternative<lambda_base>(*pos)){
              lambda_base const& b = std::get<lambda_base>(*pos);
              pos = apply_lambda(b, arguments.front(), scope);
              arguments.pop_front();
          }else if(std::holds_alternative<built_in>(*pos)){
              built_in const& b = std::get<built_in>(*pos);
              built_in::evaluator last_evaluator = b.eval;
              built_in::evaluation_result result = last_evaluator(arguments);
              while(std::holds_alternative<built_in::require_values>(result)){
                  auto const& r = std::get<built_in::require_values>(result);
                  //1. evaluate everything.
                  //Check if more arguments are needed
                  for(std::size_t index : r.arguments_needed){
                      if(index >= arguments.size()){
                          return {make_application(make_builtin(std::move(last_evaluator), b.name), arguments), true};
                      }
                  }
                  for(std::size_t index : r.arguments_needed){
                      auto [simplification, success] = try_simplify_at_top(arguments[index], scope);
                      arguments[index] = simplification;
                      if(!success)
                          return {make_application(make_builtin(std::move(last_evaluator), b.name), arguments), false};
                  }
                  last_evaluator = r.eval;
                  result = last_evaluator(arguments);
              }
              if(std::holds_alternative<built_in::nothing_to_do_t>(result)){
                  return {make_application(make_builtin([](auto&&){ return built_in::nothing_to_do; }, b.name), arguments), true};
              }else{
                  assert(std::holds_alternative<built_in::simplified>(result));
                  auto const& simplification = std::get<built_in::simplified>(result);
                  arguments.erase(arguments.begin(), arguments.begin() + simplification.arguments_read);
                  pos = sanitize_term(simplification.result, scope);
              }
          }else if(std::holds_alternative<argument>(*pos)){
              return {make_application(pos, arguments), false};
          }else{
              std::terminate(); //??? not good
          }
      }
  }
  namespace{
      void advance_id(std::string& current){ //current must be non-empty
          std::size_t pos = current.size() - 1;
          while(current[pos] == 'z'){
              current[pos] = 'a';
              if(pos == 0){
                  current = 'a' + current;
                  return;
              }else{
                  --pos;
              }
          }
          ++current[pos];
      }
      struct verbose_printing_data{
          std::ostream& o;
          std::string next_var_name = "a";
          std::unordered_map<std::size_t, std::string> var_names;
          std::string const& get_var_name(std::size_t index){
              if(!var_names.contains(index)){
                  var_names[index] = next_var_name;
                  advance_id(next_var_name);
              }
              return var_names[index];/*
              std::stringstream s;
              s << "$" << index;
              return var_names[index] = s.str();*/
          }
          void print(term t, bool parenthesize_application = false, bool parenthesize_lambda = false){
              std::visit(utility::overloaded{
                  [&](built_in const& b){
                      o << b.name;
                  },
                  [&](lambda_base const& l){
                      if(parenthesize_lambda) o << "(";
                      o << get_var_name(l.arg_index) << " -> ";
                      print(l.body);
                      if(parenthesize_lambda) o << ")";
                  },
                  [&](application const& a){
                      if(parenthesize_application) o << "(";
                      print(a.f, false, true);
                      o << " ";
                      print(a.x, true, !parenthesize_application);
                      if(parenthesize_application) o << ")";
                  },
                  [&](argument const& a){
                      o << get_var_name(a.arg_index);
                  },
                  [&](primitive const& a){
                      o << a.value;
                  }
              }, *t);
          }
      };
  };
  std::ostream& operator<<(std::ostream& o, term t){
      verbose_printing_data{o}.print(t);
      return o;
  }
  term full_simplify(term t, name_scope const& names){
      auto [pos, _] = try_simplify_at_top(t, names);
      std::deque<term> arguments;
      while(std::holds_alternative<application>(*pos)){
          application const& a = std::get<application>(*pos);
          arguments.push_front(full_simplify(a.x, names));
          pos = a.f;
      }
      if(std::holds_alternative<lambda_base>(*pos)){
          auto const& b = std::get<lambda_base>(*pos);
          name_scope names_inner = names;
          names_inner.add_name(b.arg_index);
          pos = make_lambda(full_simplify(b.body, names_inner), b.arg_index);
      }
      return make_application(pos, arguments);
  }
  namespace{
    bool compare_literal_impl(term a, term b, name_scope const& names, std::unordered_map<std::size_t, std::size_t>& renamings){
      if(a->index() != b->index()) return false;
      return std::visit(utility::overloaded{
          [&](built_in const& ab){
            return ab.name == std::get<built_in>(*b).name;
          },
          [&](lambda_base const& al){
            auto const& bl = std::get<lambda_base>(*b);
            renamings.insert(std::make_pair(al.arg_index, bl.arg_index));
            auto ret = compare_literal_impl(al.body, bl.body, names, renamings);
            renamings.erase(al.arg_index);
            return ret;
          },
          [&](application const& aa){
            auto const& ba = std::get<application>(*b);
            return compare_literal_impl(aa.f, ba.f, names, renamings) && compare_literal_impl(aa.x, ba.x, names, renamings);
          },
          [&](argument const& aa){
            auto const& ba = std::get<argument>(*b);
            if(renamings.contains(aa.arg_index)) return ba.arg_index == renamings.at(aa.arg_index);
            else return ba.arg_index == aa.arg_index;
          },
          [&](primitive const& ap){
            auto const& bp = std::get<primitive>(*b);
            return ap.value == bp.value;
          }
      }, *a);
    }
  }
  bool compare_literal(term a, term b, name_scope const& names){
    std::unordered_map<std::size_t, std::size_t> renamings;
    return compare_literal_impl(full_simplify(a, names), full_simplify(b, names), names, renamings);
  }


}
