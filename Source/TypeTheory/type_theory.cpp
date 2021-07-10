/*

#include "type_theory.hpp"

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
      ContextMorphism deeper_morphism;
      ContextMorphism bound_deepener;
      for(std::size_t i = 0; i < morphism.codomain_depth; ++i) {
        bound_deepener.substitutions.push_back(Value::Arg{morphism.codomain_depth - i});
      }
      bound_deepener.codomain_depth = morphism.codomain_depth + 1;
      for(auto const& v : morphism.substitutions) {
        deeper_morphism.substitutions.push_back(context_map(bound_deepener, v));
      }
      deeper_morphism.substitutions.push_back(Value::Arg{0});
      deeper_morphism.codomain_depth = morphism.codomain_depth + 1;

      return Value::Lambda{context_map(deeper_morphism, x.body)};
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
      return morphism.get(x.co_index);
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
          morphism.substitutions.push_back(Value::Arg{context_depth - i - 1});
        }
        morphism.substitutions.push_back(apply.x);
        morphism.codomain_depth = context_depth;
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
          o << "Arg {-" << x.co_index << "}\n";
        }
      }, *v);
    }
  };
  Helper{o}.print(0, v);
  return o;
}

*/
