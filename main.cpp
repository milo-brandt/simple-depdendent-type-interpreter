#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include "Templates/TemplateUtility.h"
#include <stdlib.h> //for rand
#include <string>
#include <functional>
#include <optional>
#include <deque>
#include <any>
#include <sstream>
//#include "lexer.hpp"
/*
value_ptr add(value_ptr f){
  auto arg = top_eval(f)->as_primitive<std::size_t>();
  std::stringstream s;
  s << "+ " << arg;
  return make_builtin(s.str(), [arg](value_ptr arg2){
    return make_primitive(top_eval(arg2)->as_primitive<std::size_t>() + arg);
  });
}*/

/*

Bad:

f : typed_value -> bool
f = \x -> if(is_function(x.type) && x.type.arg == typed_value && x.type.result(typed_value) == bool, x.value(x), true);

*/

//#include "replaceable_pointer.hpp"


/*
decidable set
*/

#include <memory>
#include <variant>

struct pair;
struct lambda_base;
struct argument;
struct integer;
struct application;
struct built_in;

using term = std::shared_ptr<std::variant<pair, built_in, lambda_base, argument, integer, application> >;
struct pair{
    term first;
    term second;
};
struct lambda_base{
    term body;
    std::size_t arg_index;
};
struct argument{
    std::size_t arg_index;
};
struct integer{
    long value;
};
struct application{
    term f;
    term x;
};
struct built_in{
    struct require_values;
    struct nothing_to_do_t{};
    static constexpr nothing_to_do_t nothing_to_do{};
    struct simplified{
        std::size_t arguments_read;
        term result;
    };
    using evaluation_result = std::variant<require_values, nothing_to_do_t, simplified>;
    using evaluator = std::function<evaluation_result(std::deque<term> const& args)>;
    struct require_values{
        std::vector<std::size_t> arguments_needed;
        evaluator eval;
    };
    evaluator eval;
    std::string name;
};

template<class... Args>
term make_term(Args&&... args){
    return std::make_shared<std::variant<pair, built_in, lambda_base, argument, integer, application> >(std::forward<Args>(args)...);
}
term make_pair(term first, term second){
    return make_term(pair{first,second});
}
term make_lambda(term body, std::size_t arg_index){
    return make_term(lambda_base{body, arg_index});
}
term make_argument(std::size_t arg_index){
    return make_term(argument{arg_index});
}
term make_integer(long value){
    return make_term(integer{value});
}
term make_application(term f, term x){
    return make_term(application{f, x});
}
term make_builtin(built_in::evaluator f, std::string name){
    return make_term(built_in{std::move(f), std::move(name)});
}

struct name_scope{
    std::unordered_set<std::size_t> used_names;
    bool contains(std::size_t name) const{ return used_names.contains(name); }
    void add_name(std::size_t name){ used_names.insert(name); }
    void erase_name(std::size_t name){ used_names.erase(name); }
    void ensure_name(std::size_t name){ if(!contains(name)) add_name(name); }
};

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
                [&](pair const& p){
                    enqueue_term(p.first);
                    enqueue_term(p.second);
                },
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
                [&](pair const& p) -> term{
                    return make_term(pair{sanitize(p.first), sanitize(p.second)});
                },
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
                [&](pair const& p) -> term{
                    return make_term(pair{substitute(p.first), substitute(p.second)});
                },
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

//base and arg are both individually valid, and no free term in either is bound in the other.
term sanitize_term(term base, name_scope used_names){
    return sanitizing_data{
        .renamings = {
            .used_names = std::move(used_names)
        }
    }.sanitize(base);
}
term apply_lambda(lambda_base const& f, term arg, name_scope const& s = {}){
    name_scope used = s;
    collect_dependent_names_of(f.body, used);
    return literal_substitution(f.body, f.arg_index, sanitize_term(arg, std::move(used)));
}
term apply_lambda(term t, term arg, name_scope const& s = {}){
    return apply_lambda(std::get<lambda_base>(*t), arg, s);
}

term make_multi_application(term base, std::deque<term> const& arguments){
    for(term arg : arguments){
        base = make_application(base, arg);
    }
    return base;
}
std::pair<term, bool> try_simplify_at_top(term t, name_scope const& scope = {}){
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
                        return {make_multi_application(make_builtin(std::move(last_evaluator), b.name), arguments), true};
                    }
                }
                for(std::size_t index : r.arguments_needed){
                    auto [simplification, success] = try_simplify_at_top(arguments[index], scope);
                    arguments[index] = simplification;
                    if(!success)
                        return {make_multi_application(make_builtin(std::move(last_evaluator), b.name), arguments), false};
                }
                last_evaluator = r.eval;
                result = last_evaluator(arguments);
            }
            if(std::holds_alternative<built_in::nothing_to_do_t>(result)){
                return {make_multi_application(make_builtin([](auto&&){ return built_in::nothing_to_do; }, b.name), arguments), true};
            }else{
                assert(std::holds_alternative<built_in::simplified>(result));
                auto const& simplification = std::get<built_in::simplified>(result);
                arguments.erase(arguments.begin(), arguments.begin() + simplification.arguments_read);
                pos = sanitize_term(simplification.result, scope);
            }
        }else{
            std::terminate(); //??? not good
        }
    }
}

struct verbose_print{
    term t;
};
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
                [&](pair const& p){
                    o << "["; print(p.first); o << ", "; print(p.second); o << "]";
                },
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
                [&](integer const& a){
                    o << a.value;
                }
            }, *t);
        }
    };
};
std::ostream& operator<<(std::ostream& o, verbose_print const& v){
    verbose_printing_data{o}.print(v.t);
    return o;
}
/*
namespace{
    struct full_simplifying_data{
        std::unordered_map<term, term> computed_values;
        name_scope names;
        term simplify(term t){
            if(computed_values.contains(t)) return computed_values[t];
            return computed_values[t] = std::visit<term>(utility::overloaded{
                [&](pair const& p) -> term{
                    return make_term(pair{simplify(p.first), simplify(p.second)});
                },
                [&](lambda_base const& l){
                    names.add_name(l.arg_index);
                    auto body = simplify(l.body);
                    names.erase_name(l.arg_index);
                    return make_term(lambda_base{body, l.arg_index});
                },
                [&](application const& a){
                    auto func = simplify(a.f);
                    if(std::holds_alternative<lambda_base>(*func)){
                        return simplify(apply_lambda(func, simplify(a.x), names)); //This line is wrong!
                    }else{
                        return make_term(application{func, simplify(a.x)});
                    }
                },
                [&](auto const&){
                    return t;
                }
            }, *t);
        }
    };
}
term full_simplify(term t){
    return full_simplifying_data{}.simplify(t);
}*/

term full_simplify(term t, name_scope const& names = {}){
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
    return make_multi_application(pos, arguments);
}


/*
nat = W(2, lambda b => if(b, 0, 1))
zero =
*/

/*
recPair(f,[a,b])=f(a,b)

recPair(f, x) = ... :(
*/
/*
#include <memory>
#include <variant>

template<class base>
class polymorphic_value{
    using copier = std::unique_ptr<base>(*)(base*);
    std::unique_ptr<base> ptr;
    copier copy;
public:
    template<std::derived_from<base> derived, class... Args> requires std::is_constructible_v<derived, Args&&...>
    polymorphic_value(std::in_place_t<derived>, Args&&... args):ptr(std::make_unique<derived>(std::forward<Args>(args)...)){}
    base* get(){ return ptr.get(); }
    base const* get() const{ return ptr.get(); }
    base* operator->(){ return get(); }
    base const* operator->() const{ return get(); }
    base& operator*(){ return *get(); }
    base const& operator*() const{ return *get; }
};*/
built_in::evaluation_result adder(std::deque<term> const& terms){
  return built_in::require_values{{0, 1}, [](std::deque<term> const& terms) -> built_in::evaluation_result{
    return built_in::simplified{2, make_integer(std::get<integer>(*terms[0]).value + std::get<integer>(*terms[1]).value)};
  }};
}


int main(){
    auto add = make_builtin(&adder, "+");
    auto plusfive = make_application(add, make_integer(5));
    auto seventeen = make_application(plusfive, make_integer(12));
    auto doubler = make_lambda(make_lambda(make_application(make_argument(0),make_application(make_argument(0), make_argument(1))),1),0);
    auto identity = make_lambda(make_argument(0), 0);
    std::cout << verbose_print{seventeen} << "\n";
    std::cout << verbose_print{apply_lambda(doubler, doubler)} << "\n";
    std::cout << verbose_print{full_simplify(make_application(doubler, plusfive))} << "\n";

    return 0;
}
