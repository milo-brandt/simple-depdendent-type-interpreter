#include "WebInterface/web_interface.hpp"
#include "ExpressionParser/parser_tree.hpp"

#include "ExpressionParser/expression_parser.hpp"

/*
Simple example with...
  1. Lambdas
  2. Product types
  3. Natural numbers
  4. Array

Terms for values...
  1. zero
  2. succ (Nat)
  3. induct (type family) (func) (type family @ 0) (nat)

  Context = Lists of types (each in context of prior list)
  Context morphism = List of values (each in same context, having type of target context)
*/
/*
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template<class... Ts>
class SharedVariant {
  std::shared_ptr<std::variant<Ts...> > data;
public:
  template<class Arg> requires (std::is_same_v<std::decay_t<Arg>, Ts> || ...)
  SharedVariant(Arg arg):data(std::make_shared<std::variant<Ts...> >(std::forward<Arg>(arg))) {}
  std::variant<Ts...> const& operator*() const {
    return *data;
  }
  std::variant<Ts...> const* get() const {
    return data.get();
  }
};
template<class... Ts>
bool operator==(SharedVariant<Ts...> const& a, SharedVariant<Ts...> const& b) {
  return a.get() == b.get();
}

namespace Type {
  struct Nat;
  struct Product;
  struct Array;

  using Any = SharedVariant<Nat, Product, Array>;
};
namespace Value {
  struct Apply;
  struct Zero;
  struct Succ;
  struct Lambda;
  struct Empty;
  struct Cons;
  struct Induct;
  struct Arg;

  using Any = SharedVariant<Apply, Zero, Succ, Lambda, Empty, Cons, Induct, Arg>;
};

namespace Type {
  struct Nat {};
  struct Product {
    Any carrier;
    Any target;
  };
  struct Array {
    Any base;
    Value::Any count;
  };
};
namespace Value {
  struct Apply {
    Any f;
    Any x;
  };
  struct Zero {};
  struct Succ {
    Any value;
  };
  struct Lambda {
    Any body;
  };
  struct Empty {};
  struct Cons {
    Any head;
    Any tail;
  };
  struct Induct {
    Any step;
    Any base;
    Any count;
  };
  struct Arg {
    std::size_t index;
  };
}
*/
/*
  A map from A to B is a list of values in B corresponding to types in A.
*/
/*
struct ContextMorphism {
  std::vector<Value::Any> substitutions;
  std::size_t domain_depth;
  Value::Any get(std::size_t index) const {
    if(index >= substitutions.size()) {
      return Value::Arg{index - substitutions.size() + domain_depth};
    } else {
      return substitutions[index];
    }
  }
};

Type::Any context_map(ContextMorphism const& morphism, Type::Any const& type);
Value::Any context_map(ContextMorphism const& morphism, Value::Any const& type);
Type::Any context_map(ContextMorphism const& morphism, Type::Any const& type) {
  return std::visit(overloaded{
    [&](Type::Nat const& x) -> Type::Any {
      return Type::Nat{};
    },
    [&](Type::Product const& x) -> Type::Any {
      return Type::Product{context_map(morphism, x.carrier), context_map(morphism, x.target)};
    },
    [&](Type::Array x) -> Type::Any {
      return Type::Array{context_map(morphism, x.base), context_map(morphism, x.count)};
    }
  }, *type);
}
Value::Any context_map(ContextMorphism const& morphism, Value::Any const& type) {
  return std::visit(overloaded{
    [&](Value::Apply const& x) -> Value::Any {
      return Value::Apply{context_map(morphism, x.f), context_map(morphism, x.x)};
    },
    [&](Value::Zero const& x) -> Value::Any {
      return Value::Zero{};
    },
    [&](Value::Succ const& x) -> Value::Any {
      return Value::Succ{context_map(morphism, x.value)};
    },
    [&](Value::Lambda const& x) -> Value::Any {
      return Value::Lambda{context_map(morphism, x.body)};
    },
    [&](Value::Empty const& x) -> Value::Any {
      return Value::Empty{};
    },
    [&](Value::Cons const& x) -> Value::Any {
      return Value::Cons{context_map(morphism, x.head), context_map(morphism, x.tail)};
    },
    [&](Value::Induct const& x) -> Value::Any {
      return Value::Induct{context_map(morphism, x.step), context_map(morphism, x.base), context_map(morphism, x.count)};
    },
    [&](Value::Arg const& x) -> Value::Any {
      return morphism.get(x.index);
    }
  }, *type);
}

Value::Any full_simplify(std::size_t context_depth, Value::Any const& value) {
  return std::visit(overloaded{
    [&](Value::Apply const& apply) -> Value::Any {
      auto f = full_simplify(context_depth, apply.f);
      if(auto const* lambda = std::get_if<Value::Lambda>(f.get())) {
        ContextMorphism morphism;
        for(std::size_t i = 0; i < context_depth; ++i) {
          morphism.substitutions.push_back(Value::Arg{i});
        }
        morphism.substitutions.push_back(apply.x);
        morphism.domain_depth = context_depth;
        return full_simplify(context_depth, context_map(morphism, lambda->body));
      }
      return Value::Apply{f, full_simplify(context_depth, apply.x)};
    },
    [&](Value::Zero const& zero) -> Value::Any {
      return Value::Zero{};
    },
    [&](Value::Succ const& succ) -> Value::Any {
      return Value::Succ{full_simplify(context_depth, succ.value)};
    },
    [&](Value::Lambda const& lambda) -> Value::Any {
      return Value::Lambda{full_simplify(context_depth + 1, lambda.body)};
    },
    [&](Value::Empty const& empty) -> Value::Any {
      return Value::Empty{};
    },
    [&](Value::Cons const& cons) -> Value::Any {
      return Value::Cons{full_simplify(context_depth, cons.head), full_simplify(context_depth, cons.tail)};
    },
    [&](Value::Induct const& induct) -> Value::Any {
      auto count = full_simplify(context_depth, induct.count);
      if(std::holds_alternative<Value::Zero>(*count)) {
        return induct.base;
      } else if(auto* succ = std::get_if<Value::Succ>(count.get())) {
        return full_simplify(context_depth, Value::Apply{Value::Apply{induct.step, succ->value}, Value::Induct{
          induct.step,
          induct.base,
          succ->value
        }});
      } else {
        return Value::Induct{
          full_simplify(context_depth, induct.step),
          full_simplify(context_depth, induct.base),
          count
        };
      }
    },
    [&](Value::Arg const& arg) -> Value::Any {
      return arg;
    }
  }, *value);
}

std::ostream& operator<<(std::ostream& o, Value::Any const& v) {
  struct Helper {
    std::ostream& o;
    void indent_by(std::size_t indent) {
      for(std::size_t i = 0; i < indent; ++i) {
        o << "  ";
      }
    }
    void print(std::size_t indent, Value::Any const& v) {
      std::visit(overloaded{
        [&](Value::Apply const& app) {
          indent_by(indent);
          o << "Apply {\n";
          print(indent + 1, app.f);
          print(indent + 1, app.x);
          indent_by(indent);
          o << "}\n";
        },
        [&](Value::Zero const& zero) {
          indent_by(indent);
          o << "Zero {}\n";
        },
        [&](Value::Succ const& succ) {
          indent_by(indent);
          o << "Succ {\n";
          print(indent + 1, succ.value);
          indent_by(indent);
          o << "}\n";
        },
        [&](Value::Lambda const& lambda) {
          indent_by(indent);
          o << "Lambda {\n";
          print(indent + 1, lambda.body);
          indent_by(indent);
          o << "}\n";
        },
        [&](Value::Empty const& empty) {
          indent_by(indent);
          o << "Empty {}\n";
        },
        [&](Value::Cons const& cons) {
          indent_by(indent);
          o << "Cons {\n";
          print(indent + 1, cons.head);
          print(indent + 1, cons.tail);
          indent_by(indent);
          o << "}\n";
        },
        [&](Value::Induct const& induct) {
          indent_by(indent);
          o << "Induct {\n";
          print(indent + 1, induct.step);
          print(indent + 1, induct.base);
          print(indent + 1, induct.count);
          indent_by(indent);
          o << "}\n";
        },
        [&](Value::Arg const& x) {
          indent_by(indent);
          o << "Arg {" << x.index << "}\n";
        }
      }, *v);
    }
  };
  Helper{o}.print(0, v);
  return o;
}

namespace Helper {
  namespace Detail {
    template<class T>
    constexpr auto wrap_value_constructor = []<class... Args>(Args&&... args) { return Value::Any{ T{std::forward<Args>(args)...} }; };
    template<class T>
    constexpr auto wrap_value_singleton = []() { static Value::Any ret = T{}; return ret; };
  }
  Value::Any apply(Value::Any f, Value::Any x) {
    return Value::Apply{f, x};
  }
  template<class... Args> requires (std::is_convertible_v<Args, Value::Any> && ...)
  Value::Any apply(Value::Any f, Value::Any x1, Value::Any x2, Args&&... args) {
    return apply(apply(f, x1), x2, std::forward<Args>(args)...);
  }
  Value::Any lambda(Value::Any body) {
    return Value::Lambda{body};
  }
  Value::Any induct(Value::Any step, Value::Any base, Value::Any count) {
    return Value::Induct{step, base, count};
  }
  Value::Any zero = Value::Zero{};
  Value::Any succ(Value::Any value) {
    return Value::Succ{value};
  }
  Value::Any arg(std::size_t index) {
    return Value::Arg{index};
  }
  Value::Any empty = Value::Empty{};
};

int main(int, char**) {
*/
  /*auto input =
  Helper::induct(
    Helper::lambda(
      Helper::lambda(
        Helper::succ(
          Helper::succ(
            Helper::arg(1)
          )
        )
      )
    ),
    Helper::zero,
    Helper::succ(
      Helper::succ(
        Helper::zero
      )
    )
  );*//*
  using namespace Helper;
  auto add =
  lambda(
    lambda(
      induct(
        lambda(
          lambda(
            succ(
              arg(3)
            )
          )
        ),
        arg(0),
        arg(1)
      )
    )
  );
  auto mul =
  lambda(
    lambda(
      induct(
        lambda(
          lambda(
            apply(
              context_map(
                ContextMorphism{
                  .domain_depth = 4
                },
                add
              ),
              arg(0),
              arg(3)
            )
          )
        ),
        zero,
        arg(1)
      )
    )
  );
  auto twice =
  lambda(
    lambda(
      apply(arg(0), apply(arg(0), arg(1)))
    )
  );
  auto input = apply(twice, twice);//apply(mul, succ(succ(succ(zero))), succ(succ(succ(zero))));
*/
  /*
  Value::Induct {
    Value::Lambda { Value::Any {
      Value::Lambda {
        Value::Succ { Value::Any {
          Value::Succ {
            Value::Arg {1}
          }
        }}
      }
    }},
    Value::Zero{},
    Value::Succ{Value::Any{Value::Succ{Value::Zero{}}}}
  };*/
/*
  std::cout << input << "\n";

  auto output = full_simplify(0, input);

  std::cout << output << "\n";

  std::cout << "Testing\n";
}
*/


#include <iostream>
#include "Utility/overloaded.hpp"

using namespace parser;

void print_tree(expression_parser::locator::Tree const& tree) {
  tree.visit(mdb::overloaded{
    [](expression_parser::locator::Apply const& apply) {
      std::cout << "Apply at: " << apply.position << "\n";
      print_tree(apply.lhs);
      print_tree(apply.rhs);
    },
    [](expression_parser::locator::Lambda const& lambda) {
      std::cout << "Lambda at: " << lambda.position << "\n";
      print_tree(lambda.body);
      if(lambda.type) print_tree(*lambda.type);
    },
    [](expression_parser::locator::Arrow const& arrow) {
      std::cout << "Arrow at: " << arrow.position << "\n";
      print_tree(arrow.domain);
      print_tree(arrow.codomain);
    },
    [](expression_parser::locator::Identifier const& identifier) {
      std::cout << "Identifier at: " << identifier.position << "\n";
    },
    [](expression_parser::locator::Hole const& hole) {
      std::cout << "Hole at: " << hole.position << "\n";
    }
  });
}

int main(int argc, char** argv) {

  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line == "q") return 0;

    std::string_view source = line;

    auto x = expression_parser::parser::expression(source);

    if(auto* error = parser::get_if_error(&x)) {
      std::cout << "Error: " << error->error << "\nAt char: " << (error->position.begin() - (std::string_view(source)).begin()) << "\n";
    } else {
      auto& success = parser::get_success(x);
      print_tree(success.value.locator);
    }
  }
}
