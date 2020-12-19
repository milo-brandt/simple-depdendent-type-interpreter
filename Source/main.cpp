#include <iostream>
/*#include "combinatorial_parser.hpp"
#include "indirect_value.hpp"
#include "template_utility.hpp"
#include <unordered_map>
#include "expression.hpp"
#include "expression_pattern.hpp"
using namespace expressions;
struct primitive_type {
  std::string what;
  bool operator==(primitive_type const&) const = default;
};
using any_expression = expression<primitive_type>;

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

struct typed_value {
  any_expression value;
  any_expression type;
};

typed_value deepen_value(typed_value input) {
  return {
    make_expression<deepen_t<primitive_type> >(1, 0, input.value),
    make_expression<deepen_t<primitive_type> >(1, 0, input.type)
  };
}
typed_value globally_normalize_value(typed_value input) {
  return {
    reduce_globally(trivial_reducer, input.value),
    reduce_globally(trivial_reducer, input.type)
  };
}*/
/*
std::optional<typed_value> checked_apply(typed_value lhs, typed_value rhs) {
  return lhs.type.reduce_to_beta_free_head(utility::overloaded{
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
*/
/*
#include "expression_parser.hpp"

std::ostream& operator<<(std::ostream& o, primitive_type const& t) {
  return o << t.what;
}
std::ostream& operator<<(std::ostream& o, typed_value const& v) {
  return o << debug_print{v.value} << " : " << debug_print{v.type};
}
*/
/*
namespace expectation {
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
  using any_data = std::variant<static_expectation, application, lambda>;
  using any = std::shared_ptr<any_data>;
  struct lambda {
    parsed_expression_tree expector;
    any_expression type;
  };
}
namespace errors {
  struct stray_is_a {
    parsed_expression_tree where;
  };
  struct invalid_function_arg_id {
    parsed_expression_tree where;
    parsed_expression_tree id_expected;
  };
  struct lambda_not_expected {
    parsed_expression_tree location;
    expectation::any expectation;
  };
  struct lambda_has_no_type {
    parsed_expression_tree location;
  };
  struct lambda_has_conflicted_type {
    parsed_expression_tree location;
    expectation::any expectation;
    any_expression expected_domain;
    any_expression explicit_domain;
  };
  struct failed_expectation {
    parsed_expression_tree location;
    expectation::any expectation;
    typed_value found;
  };
  struct lhs_not_function {
    parsed_expression_tree location;
    typed_value lhs;
  };
  struct mismatched_equality {
    parsed_expression_tree location;
    typed_value lhs;
    typed_value rhs;
  };
}*/
/*
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

primitive_type type_axiom() { return {"Type"}; }
primitive_type arrow_axiom() { return {"Arrow"}; }
primitive_type id_axiom() { return {"Id"}; }
compile_result evalute_parsed_expression_tree(parsed_expression_tree tree, std::unordered_map<std::string, typed_value> context, std::optional<any_expression> expected_type) {
  auto result = std::visit(utility::overloaded{
    [&](parsed_binop const& op) -> compile_result {
      if(op.op == binary_operator::is_a) {
        return error{tree, "unexpected ':' operator"};
      } else if(op.op == binary_operator::superposition) {
        auto result_left = evalute_parsed_expression_tree(op.lhs, context, std::nullopt);
        if(result_left.index() == 1) return result_left;
        auto func = std::get<0>(result_left);
        auto func_type_expanded_expr = reduce_to_beta_free_head(func.type);
        if(std::holds_alternative<abstract_t<primitive_type> >(func_type_expanded_expr))
          return error{tree, "left hand side of application is not function"};
        auto func_type_expanded = std::get<beta_free_application<primitive_type> >(func_type_expanded_expr);
        if(std::holds_alternative<argument_t<primitive_type> >(func_type_expanded.head))
          return error{tree, "left hand side of application is not function"};
        auto func_type_head = std::get<value_t<primitive_type> >(func_type_expanded.head).value;
        if(func_type_head != arrow_axiom() || func_type_expanded.rev_args.size() != 2)
          return error{tree, "left hand side of application is not function"};
        auto expected_domain = func_type_expanded.rev_args[1];
        auto result_right = evalute_parsed_expression_tree(op.rhs, context, expected_domain);
        if(result_right.index() == 1) return result_right;
        auto x = std::get<0>(result_right);
        return typed_value{
          .value = apply(func.value, x.value),
          .type = apply(func_type_expanded.rev_args[0], x.value)
        };
      } else if(op.op == binary_operator::equal) {
        auto result_left = evalute_parsed_expression_tree(op.lhs, context, std::nullopt);
        if(result_left.index() == 1) return result_left;
        auto expr_left = std::get<0>(result_left);
        auto result_right = evalute_parsed_expression_tree(op.rhs, context, expr_left.type);
        if(result_right.index() == 1) return result_right;
        auto expr_right = std::get<0>(result_right);
        return typed_value{
          .value = apply(apply(apply(value(id_axiom()), expr_left.type), expr_left.value), expr_right.value),
          .type = value(type_axiom())
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
        auto domain_result = evalute_parsed_expression_tree(true_lhs, context, value(type_axiom()));
        if(domain_result.index() == 1) return domain_result;
        auto domain = std::get<0>(domain_result);
        for(auto& [name, value] : context) {
          value = deepen_value(value);
        }
        if(arg_name) {
          context.insert_or_assign(*arg_name, typed_value{
            .value = argument<primitive_type>(0),
            .type = make_expression<deepen_t<primitive_type> >(1, 0, domain.value)
          });
        }
        auto codomain_result = evalute_parsed_expression_tree(op.rhs, context, value(type_axiom()));
        if(codomain_result.index() == 1) return codomain_result;
        auto codomain = std::get<0>(codomain_result);
        return typed_value{
          .value = apply(apply(value(arrow_axiom()), domain.value), abstract(codomain.value)),
          .type = value(type_axiom())
        };
      }
      std::terminate();
    },
    [&](parsed_unop const& op) -> compile_result {
      std::string arg_name{op.op.arg_name};
      std::optional<std::pair<any_expression, any_expression> > expected_domain_and_codomain;
      if(expected_type) {
        auto func_type_expanded_expr = reduce_to_beta_free_head(*expected_type);
        if(std::holds_alternative<abstract_t<primitive_type> >(func_type_expanded_expr))
          return error{tree, "lambda found, but wasn't expecting function"};
        auto func_type_expanded = std::get<beta_free_application<primitive_type> >(func_type_expanded_expr);
        if(std::holds_alternative<argument_t<primitive_type> >(func_type_expanded.head))
          return error{tree, "lambda found, but wasn't expecting function"};
        auto func_type_head = std::get<value_t<primitive_type> >(func_type_expanded.head).value;
        if(func_type_head != arrow_axiom() || func_type_expanded.rev_args.size() != 2)
          return error{tree, "lambda found, but wasn't expecting function"};
        expected_domain_and_codomain = std::make_pair(func_type_expanded.rev_args[1], func_type_expanded.rev_args[0]);
      }
      std::optional<any_expression> explicit_domain;
      if(op.op.arg_type) {
        auto arg_result = evalute_parsed_expression_tree(op.op.arg_type, context, value(type_axiom()));
        if(arg_result.index() == 1) return arg_result;
        explicit_domain = std::get<0>(arg_result).value;
      }
      if(!explicit_domain && !expected_domain_and_codomain)
        return error{tree, "lambda argument without type"};
      if(explicit_domain && expected_domain_and_codomain && !deep_compare(trivial_reducer, *explicit_domain, expected_domain_and_codomain->first))
        return error{tree, "explicit lambda argument type does not match expected type"};
      if(!explicit_domain)
        explicit_domain = expected_domain_and_codomain->first;
      auto arg_type = *explicit_domain;
      for(auto& [name, value] : context) {
        value = deepen_value(value);
      }
      context.insert_or_assign(arg_name, typed_value{
        .value = argument<primitive_type>(0),
        .type = make_expression<deepen_t<primitive_type> >(1, 0, arg_type)
      });
      std::optional<any_expression> expected_codomain;
      if(expected_domain_and_codomain)
        expected_codomain = apply(expected_domain_and_codomain->second, argument<primitive_type>(0));
      auto body_result = evalute_parsed_expression_tree(op.operand, context, expected_codomain);
      if(body_result.index() == 1) return body_result;
      auto body = std::get<0>(body_result);
      return typed_value{
        .value = abstract(body.value),
        .type = apply(apply(value(arrow_axiom()), arg_type), abstract(body.type))
      };
    },
    [&](operator_tree::ident const& id) -> compile_result {
      if(!context.contains(std::string(id.id))) return error{tree, "unrecognized id"};
      return context.at(std::string(id.id));
    }
  },
  *tree);
  if(result.index() == 1) return result;
  if(expected_type && !deep_compare(trivial_reducer, std::get<0>(result).type, *expected_type)) {
    return error{tree, "wrong type"};
  }
  return result;
}

namespace command {
  struct set {
    std::string id;
    parsed_expression_tree value;
  };
  struct axiom {
    std::string id;
    parsed_expression_tree type;
  };
  struct evaluate {
    parsed_expression_tree value;
  };
  struct rule {
    std::vector<std::tuple<std::string_view, parsed_expression_tree> > vars;
    parsed_expression_tree lhs;
    parsed_expression_tree rhs;
  };
  using any = std::variant<set, axiom, evaluate, rule>;
};
#include <cstdint>
#include <fstream>
struct raw_data {
  std::unique_ptr<std::uint8_t[]> data;
  std::size_t size;
};

std::string read_file(std::string const& path) {
  std::ifstream input(path);
  assert(input);
  input.seekg(0, std::ios_base::end);
  auto length = input.tellg();
  input.seekg(0);
  auto buffer = raw_data{
    .data = std::make_unique<std::uint8_t[]>(length),
    .size = (std::size_t)length
  };
  input.read((char*)buffer.data.get(), length);
  return std::string((char*)buffer.data.get(), length);
}
*/
/*
Possibilities:
  1. Can yield.
  2. Can call another function with compatible signature.
*/
/*
Interesting idea: given a coroutine with some signals, could run it from another, routing some signals all the way out and capturing others
  - just need some operator "consuming" the right signals?
*/
/*struct lifetime_tester {
  lifetime_tester() {
    std::cout << "Constructed." << this << "\n";
  }
  ~lifetime_tester() {
    std::cout << "Destructed" << this << ".\n";
  }
};*/

#include "lifted_state_machine.hpp"


namespace lifted_state_machine::example {
    struct take_int_t{};
    constexpr take_int_t take_int{};
    struct yield_int{
      int x;
    };
    struct simple_definition : coro_base<simple_definition> {
      using starter_base = resume_handle<>;
      struct state{};
      using paused_type = std::variant<std::pair<int, resume_handle<> >, resume_handle<int>, int>;
      static starter_base create_object(resume_handle<> handle) {
        return handle;
      }
      static auto on_await(take_int_t, state&, waiting_handle&& handle) {
        return std::move(handle).resuming_result<int>([](auto resumer){ return resumer; });
      }
      static auto on_yield(int x, state&, waiting_handle&& handle) {
        return std::move(handle).resuming_result([&](auto resumer){ return std::make_pair(x, std::move(resumer)); });
      }
      /*static void on_return_value(int x, returning_handle&& handle) {
        return std::move(handle).result(x);
      }*/
      static void on_return_void(state&, returning_handle&& handle) {
        return std::move(handle).result(17);
      }
    };
    coro_type<simple_definition> the_thing() {
      //lifetime_tester t;
      int acc = 0;
      while(true) {
        acc += co_await take_int;
        if(acc == 289) throw int(5);
        if(acc > 500) acc = 1;
        co_yield acc;
      }
    }
}
lifted_state_machine::coro_type<lifted_state_machine::example::simple_definition> the_thing_plus() {
  while(true) {
    int ct = co_await lifted_state_machine::example::take_int;
    for(int i = 0; i < ct; ++i) {
      co_yield i;
    }
  }
}

struct parse_id_t{};
constexpr parse_id_t parse_id{};

struct lifetime_tester {
  void* ptr;
  lifetime_tester():ptr(this) {
    std::cout << "Constructed at " << this << "\n";
  }
  lifetime_tester(lifetime_tester const& other):ptr(this) {
    std::cout << "Copy constructed from " << &other << " to " << this << "\n";
  }
  lifetime_tester(lifetime_tester&& other):ptr(this) {
    std::cout << "Move constructed from " << &other << " to " << this << "\n";
  }
  lifetime_tester& operator=(lifetime_tester const& other) {
    std::cout << "Copy assigned from " << &other << " to " << this << "\n";
    return *this;
  }
  lifetime_tester& operator=(lifetime_tester&& other) {
    std::cout << "Move assigned from " << &other << " to " << this << "\n";
    return *this;
  }
  ~lifetime_tester() {
    std::cout << "Destructed at " << this << "\n";
  }
  void output_position() const {
    std::cout << "Poked at " << this << " with ptr " << ptr << "\n";
  }
};

struct tag {
  std::string what;
  lifetime_tester tester;
};

struct parse_error {
  std::string_view position;
  std::string message;
};
template<class T>
struct parsing_function : lifted_state_machine::coro_base<parsing_function<T> > {
  using coro_base =  lifted_state_machine::coro_base<parsing_function<T> >;
  template<class V = void>
  using resume_handle = coro_base::template resume_handle<V>;
  using state = std::string_view;
  using paused_type = std::variant<parse_error, std::pair<T, std::string_view> >;
  class starter_base {
    resume_handle<> handle;
    friend parsing_function;
    starter_base(resume_handle<> handle):handle(std::move(handle)){}
  public:
    starter_base() = default;
    auto begin(std::string_view str) && {
      handle.state() = str;
      return std::move(handle).resume();
    }
  };
  static starter_base create_object(resume_handle<> handle) {
    return starter_base(std::move(handle));
  }
  static coro::variant_awaiter<void, lifted_state_machine::immediate_awaiter<>, lifted_state_machine::await_with_no_resume<> >
    on_await(tag const& t, state& state, coro_base::waiting_handle&& handle) {
    t.tester.output_position();
    if(state.starts_with(t.what)) {
      state.remove_prefix(t.what.size());
      return std::move(handle).immediate_result();
    } else {
      return std::move(handle).destroying_result(parse_error{state, "expected: "+ t.what});
    }
  }
  static auto on_await(lifetime_tester const& t, state&, coro_base::waiting_handle&& handle) {
    return std::move(handle).immediate_result(17);
  }
  static void on_return_value(T value, state& state, coro_base::returning_handle&& handle) {
    return std::move(handle).result(std::make_pair(std::move(value), state));
  }
};
template<class T>
using parsing_routine = lifted_state_machine::coro_type<parsing_function<T> >;

parsing_routine<int> checker() {
  //std::string str("hi");
  //tag t{str};
  co_await tag{"hey"}; //lifetime_tester{};//tag{std::move(str)};
  //tag second{" "};
  //co_await second;
  //tag third{"world"};
  //co_await third;
  co_return 17;
}

/*
  For recursive duty, just need to allow returns to propagate back?
  ...that's a bit tricky (because we don't separate returns), but should be possible - just, if "done" is set,
  ...send the value back via a callback - somehow have to keep track of top of stack too
*/

/*struct tag {
  std::string str;
  lifetime_tester tester;
};
struct routine{
    struct promise_type{
        routine get_return_object(){
            return {};
        }
        auto initial_suspend(){
            return std::suspend_never();
        }
        auto final_suspend(){
            return std::suspend_never();
        }
        void return_void(){}
        void unhandled_exception(){
          std::terminate();
        }
        auto await_transform(tag&& t) {
          t.tester.output_position();
          return std::suspend_never();
        }
    };
};*/


int main() {
  auto x = checker();
  auto result = std::move(x).begin("hi worl");
  if(auto* err = std::get_if<0>(&result)) {
    std::cout << "Parsed until " << err->position << "\n";
    std::cout << "Error: " << err->message << "\n";
  } else {
    auto& value = std::get<1>(result);
    std::cout << "Result: " << value.first << "\n";
    std::cout << "Remaining: " << value.second << "\n";
  }
  /*
  auto a = lifted_state_machine::example::the_thing();
  auto x = the_thing_plus();
  using state = lifted_state_machine::example::simple_definition::paused_type;
  auto b = std::move(a).resume();
  auto y = std::move(x).resume();
  try{
  while(
    std::visit(utility::overloaded{
      [&](std::pair<int, lifted_state_machine::example::simple_definition::resume_handle<> > p) {
        std::cout << "Yielded: " << p.first << "\n";
        y = std::move(p.second).resume();
        return true;
      },
      [&](lifted_state_machine::example::simple_definition::resume_handle<int> p) {
        int x;
        bool done = false;
        while(std::visit(utility::overloaded{
          [&](std::pair<int, lifted_state_machine::example::simple_definition::resume_handle<> > p) {
            x = p.first;
            b = std::move(p.second).resume();
            return false;
          },
          [&](lifted_state_machine::example::simple_definition::resume_handle<int> p) {
            std::cout << "Enter an integer: ";
            int z;
            std::cin >> z;
            if(z == 17){
              done = true;
              return false;
            };
            b = std::move(p).resume(z);
            return true;
          },
          [&](int x) {
            done = true;
            return false;
          }
        }, std::move(b)));
        if(done) return false;
        y = std::move(p).resume(x);
        return true;
      },
      [&](int x) {
        std::cout << "Returned: " << x << "\n";
        return false;
      }
    }, std::move(y))
  );
}catch(int x) {
  std::cout << "Caught " << x << "\n";
}*/
  /*{
    using namespace expressions;
    auto s1 = abstract(abstract(apply(argument<int>(1), apply(argument<int>(1), argument<int>(0)))));
    auto s2 = apply(s1, s1);
    auto s = apply(s2, s2);
    std::cout << debug_print<int>{s} << "\n";
    std::cout << debug_print<int>{reduce_globally(trivial_reducer, s)} << "\n";

  }
  std::unordered_map<std::string, typed_value> ctx;
  ctx.insert(std::make_pair("Type", typed_value{
    .value = value(type_axiom()),
    .type = value(type_axiom())
  }));
  auto parse_expr = expression_parser();
  auto command_parser = either{
    whitespace_sequence(recognizer("axiom"), parse_identifier, recognizer(":"), parse_expr)
      > [](auto tup) -> command::any{ auto& [_1, id, _2, expr] = tup; return command::axiom{std::string(id), expr}; },
    whitespace_sequence(recognizer("let"), parse_identifier, recognizer(":="), parse_expr)
      > [](auto tup) -> command::any{ auto& [_1, id, _2, expr] = tup; return command::set{std::string(id), expr}; },
    whitespace_sequence(
      recognizer("rule"),
      optional(surrounded(recognizer("("),
        separated_sequence(separated_pair(white_surround(parse_identifier), white_surround(recognizer(":")), white_surround(parse_expr)), white_surround(recognizer(","))),
      recognizer(")"))),
      parse_expr, //somehow eats the : if allowed to???
      recognizer(":="),
      parse_expr)
      > [](auto tup) -> command::any{ auto& [_1, vec, lhs, _2, rhs] = tup; return command::rule{vec?*vec:std::vector<std::tuple<std::string_view, parsed_expression_tree> >{}, lhs, rhs}; },
    white_surround(parse_expr) > [](auto expr) -> command::any{ return command::evaluate{expr}; }
  } > utility::collapse_variant_legs;
  auto try_compile = [&](parsed_expression_tree t, std::optional<any_expression> type = {}) -> std::optional<typed_value> {
    auto compiled = evalute_parsed_expression_tree(t, ctx, type);
    if(compiled.index() == 1) {
      std::cout << "Compilation failed.\n";
      auto err = std::get<1>(compiled);
      std::cout << "Error at: " << operator_tree::get_span_of(err.where) << "\n";
      std::cout << "Error: " << err.what << "\n";
      return std::nullopt;
    } else {
      return std::get<0>(compiled);
    }
  };
  auto accept_result = utility::overloaded{
    [&](command::set set) {
      auto x = try_compile(set.value);
      if(x) {
        auto v = globally_normalize_value(*x);
        std::cout << "Set " << set.id << " to " << v << "\n";
        ctx.insert(std::make_pair(set.id, v));
      }
    },
    [&](command::axiom axiom) {
      auto x = try_compile(axiom.type, value(type_axiom()));
      if(x) {
        auto v = globally_normalize_value(*x);
        std::cout << "Declared axiom " << axiom.id << ": " << debug_print{v.value} << "\n";
        ctx.insert(std::make_pair(axiom.id, typed_value{
          .value = value(primitive_type{axiom.id}),
          .type = v.value
        }));
      }
    },
    [&](command::evaluate v) {
      auto x = try_compile(v.value);
      if(x) {
        std::cout << "Read: " << *x << "\n";
        std::cout << "Equals: " << globally_normalize_value(*x) << "\n";
      }
    },
    [&](command::rule v) {
      std::cout << "Rule ignored.\n";
    }
  };
  auto file_string = read_file("source.txt");
  auto file_commands = (*suffixed{command_parser, recognizer{";"}}).parse(file_string);
  if(file_commands.index() == 0) {
    std::cout << "Failed to parse file\n";
  } else {
    std::cout << "Unparsed section of file: " << std::get<1>(file_commands).remaining << "\n";
    for(auto command: std::get<1>(file_commands).ret) {
      std::visit(accept_result, command);
    }
  }


  while(true) {
    std::cout << "Enter a string.\n";
    std::string str;
    std::getline(std::cin, str);
    if(str == "quit") return 0;
    auto result = command_parser.parse(str);
    if(result.index() == 0) {
      std::cout << "Failed to parsed " << str << "\n";
    } else {
      std::visit(accept_result, std::get<1>(result).ret);
      //std::cout << "Parsed except: " << std::get<1>(result).remaining << "\n";*/
      /*
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
      }*/

    //}
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
  //}

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
/*
  any_expression a(apply{argument{0}, argument{1}});
  any_expression b = a;
  any_expression doubler = lambda{any_expression{lambda{apply{argument{1}, apply{argument{1}, argument{0}}}}}};
  any_expression quad = apply{doubler, doubler};
  //any_expression twofitysix = apply{quad, quad};

  std::cout << quad << "\n";
  std::cout << quad.normalize_locally() << "\n";
  std::cout << quad.normalize_globally() << "\n";
  std::cout << deep_compare(quad, quad.normalize_globally()) << "\n";*/
  //output_expr(totally_simplify(doubler)); std::cout << "\n
}
