#include <iostream>
#include "expression.hpp"
/*#include "lifted_state_machine.hpp"*/
#include "combinatorial_parser.hpp"
#include "indirect_value.hpp"
#include "template_utility.hpp"
#include <unordered_map>

struct axiom_registry {
  std::vector<std::string> names;
  std::size_t register_axiom(std::string name) {
    auto ret = names.size();
    names.push_back(std::move(name));
    return ret;
  }
};
axiom_registry& main_registry() {
  static axiom_registry ret;
  return ret;
}
std::size_t type_axiom() { static std::size_t axiom = main_registry().register_axiom("type"); return axiom; }
std::size_t arrow_axiom() { static std::size_t axiom = main_registry().register_axiom("->"); return axiom; }
std::size_t id_axiom() { static std::size_t axiom = main_registry().register_axiom("id"); return axiom; }

struct typed_value {
  any_expression value;
  any_expression type;
};

typed_value deepen_value(typed_value input) {
  return {
    deepened_expression{1, 0, input.value},
    deepened_expression{1, 0, input.type}
  };
}
typed_value globally_normalize_value(typed_value input) {
  return {
    input.value.normalize_globally(),
    input.type.normalize_globally()
  };
}

std::optional<typed_value> checked_apply(typed_value lhs, typed_value rhs) {
  return lhs.type.normalize_locally_then(utility::overloaded{
    [&](auto const&) -> std::optional<typed_value> {
      return std::nullopt; //lhs isn't a function
    },
    [&](normalized_to_axiom_t const& axiom) -> std::optional<typed_value> {
      if(axiom.a.index != arrow_axiom() || axiom.rev_args.size() != 2) return std::nullopt;
      auto domain = axiom.rev_args[1];
      auto codomain = axiom.rev_args[0];
      if(!deep_compare(rhs.type, domain)) return std::nullopt; //rhs doesn't match
      return typed_value{
        apply{lhs.value, rhs.value},
        apply{codomain, rhs.value}
      };
    }
  });
}

#include "expression_parser.hpp"

std::ostream& operator<<(std::ostream& o, typed_value const& v) {
  return o << v.value << " : " << v.type;
}
/*
namespace expectations {
  struct static_expectation {
    parsed_expression_tree expector;
    std::string message;
    any_expression type;
  };
  struct application {
    parsed_expression_tree expector;
    typed_value lhs;
    any_expression type;
  };
  struct lambda;
  struct any_data = std::variant<static_expectation, application, lambda>;
  struct any = std::shared_ptr<any_data>;
  struct lambda {
    parsed_expression_tree expector;

    any_expression type;
  };
}
namespace errors {
  struct failed_expectation {
    parsed_expression_tree location;
    any_expression expectation;
  };
}*/
struct error {
  parsed_expression_tree where;
  std::string what;
};
using compile_result = std::variant<typed_value, error>;
void print_context(std::unordered_map<std::string, typed_value> const& context) {
  for(auto const& [name, value] : context) {
    std::cout << "\t" << name << ": " << value << "\n";
  }
}
compile_result evalute_parsed_expression_tree(parsed_expression_tree tree, std::unordered_map<std::string, typed_value> context, std::optional<any_expression> expected_type) {
  //std::cout << "Parsing at " << operator_tree::get_span_of(tree) << " with context: \n";
  //print_context(context);
  auto result = std::visit(utility::overloaded{
    [&](parsed_binop const& op) -> compile_result {
      if(op.op == binary_operator::is_a) {
        return error{tree, "unexpected ':' operator"};
      } else if(op.op == binary_operator::superposition) {
        auto result_left = evalute_parsed_expression_tree(op.lhs, context, std::nullopt);
        if(result_left.index() == 1) return result_left;
        std::optional<any_expression> expected_domain;
        std::get<0>(result_left).type.normalize_locally_then(utility::overloaded{
          [](auto const&){},
          [&](normalized_to_axiom_t const& a) {
            if(a.a.index != arrow_axiom() || a.rev_args.size() != 2) return;
            expected_domain = a.rev_args[1];
          }
        });
        if(!expected_domain) return error{tree, "left hand side of application is not function"};
        auto result_right = evalute_parsed_expression_tree(op.rhs, context, expected_domain);
        if(result_right.index() == 1) return result_right;
        auto ret = checked_apply(std::get<0>(result_left), std::get<0>(result_right));
        if(!ret) return error{tree, "mismatched types in application"};
        return *ret;
      } else if(op.op == binary_operator::equal) {
        auto result_left = evalute_parsed_expression_tree(op.lhs, context, std::nullopt);
        if(result_left.index() == 1) return result_left;
        auto result_right = evalute_parsed_expression_tree(op.rhs, context, std::nullopt);
        if(result_right.index() == 1) return result_right;
        auto expr_left = std::get<0>(result_left);
        auto expr_right = std::get<0>(result_right);
        if(!deep_compare(expr_left.type,expr_right.type)) return error{tree, "mismatched types in equality"};
        return typed_value{
          .value = apply{apply{apply{axiom{id_axiom()}, expr_left.type}, expr_left.value}, expr_right.value},
          .type = axiom{type_axiom()}
        };
      } else if(op.op == binary_operator::arrow) {
        parsed_expression_tree true_lhs;
        std::optional<std::string> arg_name;
        if(op.lhs->index() == 0 && std::get<0>(*op.lhs).op == binary_operator::is_a) {
          auto const& lhs_op = std::get<0>(*op.lhs);
          true_lhs = lhs_op.rhs;
          if(lhs_op.lhs->index() != 2) return error{tree, "in the x:Type -> ... construction, the value x must be an identifier"};
          arg_name = std::string(std::get<2>(*lhs_op.lhs).id);
        } else {
          true_lhs = op.lhs;
        }
        auto domain_result = evalute_parsed_expression_tree(true_lhs, context, axiom{type_axiom()});
        if(domain_result.index() == 1) return domain_result;
        auto domain = std::get<0>(domain_result);
        for(auto& [name, value] : context) {
          value = deepen_value(value);
        }
        if(arg_name) {
          context.insert_or_assign(*arg_name, typed_value{
            .value = argument{0},
            .type = deepened_expression{1, 0, domain.value}
          });
        }
        auto codomain_result = evalute_parsed_expression_tree(op.rhs, context, axiom{type_axiom()});
        if(codomain_result.index() == 1) return codomain_result;
        auto codomain = std::get<0>(codomain_result);
        return typed_value{
          .value = apply{apply{axiom{arrow_axiom()}, domain.value}, lambda{codomain.value}},
          .type = axiom{type_axiom()}
        };
      }
      std::terminate();
    },
    [&](parsed_unop const& op) -> compile_result {
      std::string arg_name{op.op.arg_name};
      std::optional<std::pair<any_expression, any_expression> > expected_domain_and_codomain;
      if(expected_type) {
        expected_type->normalize_locally_then(utility::overloaded{
          [](auto const&){},
          [&](normalized_to_axiom_t const& a) {
            if(a.a.index != arrow_axiom() || a.rev_args.size() != 2) return;
            expected_domain_and_codomain = std::make_pair(a.rev_args[1], a.rev_args[0]);
          }
        });
        if(!expected_domain_and_codomain) return error{tree, "lambda found where expected type is not function"};
      }
      std::optional<any_expression> explicit_domain;
      if(op.op.arg_type) {
        auto arg_result = evalute_parsed_expression_tree(op.op.arg_type, context, axiom{type_axiom()});
        if(arg_result.index() == 1) return arg_result;
        explicit_domain = std::get<0>(arg_result).value;
      }
      if(!explicit_domain && !expected_domain_and_codomain)
        return error{tree, "lambda argument without type"};
      if(explicit_domain && expected_domain_and_codomain && !deep_compare(*explicit_domain, expected_domain_and_codomain->first))
        return error{tree, "explicit lambda argument type does not match expected type"};
      if(!explicit_domain)
        explicit_domain = expected_domain_and_codomain->first;
      auto arg_type = *explicit_domain;
      for(auto& [name, value] : context) {
        value = deepen_value(value);
      }
      context.insert_or_assign(arg_name, typed_value{
        .value = argument{0},
        .type = deepened_expression{1, 0, arg_type}
      });
      std::optional<any_expression> expected_codomain;
      if(expected_domain_and_codomain)
        expected_codomain = apply{expected_domain_and_codomain->second, argument{0}};
      auto body_result = evalute_parsed_expression_tree(op.operand, context, expected_codomain);
      if(body_result.index() == 1) return body_result;
      auto body = std::get<0>(body_result);
      return typed_value{
        .value = lambda{body.value},
        .type = apply{apply{axiom{arrow_axiom()}, arg_type}, lambda{body.type}}
      };
    },
    [&](operator_tree::ident const& id) -> compile_result {
      if(!context.contains(std::string(id.id))) return error{tree, "unrecognized id"};
      return context.at(std::string(id.id));
    }
  },
  *tree);
  if(result.index() == 1) return result;
  if(expected_type && !deep_compare(std::get<0>(result).type, *expected_type)) {
    return error{tree, "wrong type"};
  }
  return result;
}

int main() {
  std::unordered_map<std::string, typed_value> ctx;
  ctx.insert(std::make_pair("type", typed_value{
    .value = axiom{type_axiom()},
    .type = axiom{type_axiom()}
  }));
  auto x = expression_parser();
  while(true) {
    std::cout << "Enter a string.\n";
    std::string str;
    std::getline(std::cin, str);
    if(str == "quit") return 0;
    auto result = (either{
      sequence{surrounded{surrounded{*whitespace, recognizer{"set"}, *whitespace}, identifier, surrounded{*whitespace, recognizer{":="}, *whitespace}}, x}
        > [](auto ret) -> std::tuple<std::optional<std::string_view>, parsed_expression_tree>{ return {std::get<0>(ret), std::get<1>(ret)}; },
      x > [](auto ret) -> std::tuple<std::optional<std::string_view>, parsed_expression_tree>{ return {std::nullopt, ret}; }
    } > utility::collapse_variant_legs).parse(str);
    if(result.index() == 0) {
      std::cout << "Failed\n";
    } else {
      std::cout << "Parsed except: " << std::get<1>(result).remaining << "\n";
      std::cout << "Parse Tree: " << std::get<1>(std::get<1>(result).ret) << "\n";
      auto compiled = evalute_parsed_expression_tree(std::get<1>(std::get<1>(result).ret), ctx, std::nullopt);
      if(compiled.index() == 1) {
        std::cout << "Compilation failed.\n";
        auto err = std::get<1>(compiled);
        std::cout << "Error at: " << operator_tree::get_span_of(err.where) << "\n";
        std::cout << "Error: " << err.what << "\n";
      } else {
        auto v = globally_normalize_value(std::get<0>(compiled));
        std::cout << "Value: " << std::get<0>(compiled) << "\n";
        std::cout << "Simplified: " << v << "\n";
        if(std::get<0>(std::get<1>(result).ret)) {
          std::string name(*std::get<0>(std::get<1>(result).ret));
          ctx.insert_or_assign(name, v);
        }
      }

    }
    /*std::unordered_map<std::string, typed_value> context;

    auto func_type = apply{
      apply{
        axiom{arrow_axiom()},
        axiom{type_axiom()}
      },
      lambda{
        apply{
          apply{
            axiom{arrow_axiom()},
            apply{
              apply{
                axiom{arrow_axiom()},
                argument{0}
              },
              lambda{
                axiom{type_axiom()}
              }
            }
          },
          lambda{
            axiom{type_axiom()}
          }
        }
      }
    };


    context.insert(std::make_pair("type", typed_value{axiom{type_axiom()}, axiom{type_axiom()} }));
    context.insert(std::make_pair("hom", typed_value{axiom{arrow_axiom()}, func_type}));

    if(str == "quit") return 0;*/
    /*auto result = parse_expression(context, {}).parse(str);
    if(result.index() == 0) {
      std::cout << "Failed.\n";
      continue;
    } else {
      auto value = std::get<1>(result);
      std::cout << "Unparsed section: " << value.remaining << "\n";
      std::cout << "Value: " << value.ret << "\n";
      std::cout << "Simplified: " << value.ret.value.normalize_globally() << " : " << value.ret.type.normalize_globally() << "\n";
    }*/
  }

  /*auto parser = read_char{[](char x){ return read_char{[x](char y){ return ret{char(x + y - '0')}; }}; }};
  auto next = step(parser, 'a');
  auto nextt = step(next, '1');
  std::cout << nextt.c << "\n";*/
/*
  constexpr auto first = ARG(x);
  constexpr auto second = ARG(y);
  static_assert(first.get_value() == second.get_value());
  std::cout << first.get_value() << "\n";
  std::cout << second.get_value() << "\n";*/
  /*std::string str = "this_id (is up a tree)";
  auto result = helpers::expression.parse(str);
  std::cout << result.index() << "\n";
  auto y = std::get<1>(result).ret;
  std::cout << y << "\n";*/
  /*
  auto handle = simple_coro();
  auto next = handle();
  try{
  while(next.index()) {
    next = std::visit<decltype(next)>([&]<class T>(T&& next) -> decltype(handle()) {
      if constexpr(std::is_same_v<T, std::monostate>) {
        std::terminate();
      } else if constexpr(std::is_same_v<typename T::first_type::resume_type, int>){
        int x;
        std::cout << "Please enter an integer.\n";
        std::cin >> x;
        return next.second(x);
      } else {
        std::string str;
        std::cout << "Please enter a string.\n";
        std::cin >> str;
        if(str == "Haha") {
          return {};
        }
        return next.second(str);
      }
    }, std::move(next));
  }
}catch(int x) {
  std::cout << "Caught " << x << "\n";
}*/
  //auto x = printer<utility::class_list<void*, std::string> >{};
  //x.print_and_return(1.5);

  any_expression a(apply{argument{0}, argument{1}});
  any_expression b = a;
  any_expression doubler = lambda{any_expression{lambda{apply{argument{1}, apply{argument{1}, argument{0}}}}}};
  any_expression quad = apply{doubler, doubler};
  //any_expression twofitysix = apply{quad, quad};

  std::cout << quad << "\n";
  std::cout << quad.normalize_locally() << "\n";
  std::cout << quad.normalize_globally() << "\n";
  std::cout << deep_compare(quad, quad.normalize_globally()) << "\n";
  //output_expr(totally_simplify(doubler)); std::cout << "\n
}
