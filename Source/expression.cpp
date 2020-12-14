#include "expression.hpp"
#include "template_utility.hpp"
#include <numeric> //std::accumulate
#include <algorithm>

struct apply;
struct lambda;
struct argument;
class any_expression;
std::size_t& any_expression::get_ref_count() const { return *(std::size_t*)data; }
any_expression::vtable_t const* any_expression::get_vptr() const { return *(vtable_t**)(data + sizeof(std::size_t)); }
void* any_expression::get_data_ptr() const { return data + sizeof(std::size_t) + sizeof(vtable_t*); }
any_expression::any_expression(any_expression const& other):data(other.data) {
  if(data) ++get_ref_count();
}
any_expression::any_expression(any_expression&& other):data(other.data) {
  other.data = nullptr;
}
any_expression& any_expression::operator=(any_expression const& other) {
  any_expression copy{other};
  this->~any_expression();
  new (this) any_expression(std::move(copy));
  return *this;
}
any_expression& any_expression::operator=(any_expression&& other) {
  std::swap(data, other.data);
  return *this;
}
std::size_t any_expression::get_primitive_type() const { return get_vptr()->primitive_type; }
any_expression any_expression::step_towards_primitive() const{
  assert(!get_primitive_type());
  return get_vptr()->reduce(get_data_ptr());
}
any_expression any_expression::reduce_to_primitive() const {
  any_expression ret = *this;
  while(!ret.get_primitive_type()) ret = ret.step_towards_primitive();
  return ret;
}
any_expression any_expression::normalize_locally() const {
  return reduce_to_primitive_then(utility::overloaded{
    [&](apply const& a) -> any_expression {
      return a.f.normalize_locally().reduce_to_primitive_then(utility::overloaded{
        [&](apply const& aa) -> any_expression {
          return apply{aa, a.x};
        },
        [&](lambda const& l) -> any_expression {
          return any_expression(substitute_expression{0, a.x, l.body}).normalize_locally();
        },
        [&](argument const& aa) -> any_expression {
          return apply{aa, a.x};
        },
        [&](axiom const& aa) -> any_expression {
          return apply{aa, a.x};
        }
      });
    },
    [&](lambda const& l) -> any_expression {
      return l;
    },
    [&](argument const& a) -> any_expression {
      return a;
    },
    [&](axiom const& a) -> any_expression {
      return a;
    }
  });
}
any_expression any_expression::normalize_globally() const {
  return normalize_locally_then(utility::overloaded{
    [&](lambda const& l) -> any_expression {
      return lambda{l.body.normalize_globally()};
    },
    [&](normalized_to_axiom_t const& l) -> any_expression {
      return std::accumulate(l.rev_args.rbegin(), l.rev_args.rend(), any_expression{l.a}, [](any_expression lhs, any_expression rhs) -> any_expression{ return apply{lhs, rhs.normalize_globally()}; });
    },
    [&](normalized_to_arg_t const& l) -> any_expression {
      return std::accumulate(l.rev_args.rbegin(), l.rev_args.rend(), any_expression{l.a}, [](any_expression lhs, any_expression rhs) -> any_expression{ return apply{lhs, rhs.normalize_globally()}; });
    }
  });
}

any_expression::~any_expression() {
  if(data && --get_ref_count() == 0) {
    get_vptr()->destroy(get_data_ptr());
    free(data);
  }
};

any_expression bind_expression::reduce() const {
  return target.reduce_to_primitive_then(utility::overloaded{
    [&](apply const& a) -> any_expression {
      return apply{ bind_expression{ first_depth, bound, a.f } , bind_expression{ first_depth, bound, a.x } };
    },
    [&](lambda const& l) -> any_expression {
      return lambda{ bind_expression{ first_depth + 1, bound, l.body } };
    },
    [&](argument const& a) -> any_expression {
      if(a.arg > first_depth) return argument{ a.arg - first_depth };
      if(first_depth - a.arg >= bound->size()) return argument{ a.arg };
      return (*bound)[first_depth - a.arg];
    },
    [&](axiom const& a) -> any_expression {
      return a;
    }
  });
}
any_expression deepened_expression::reduce() const {
  return target.reduce_to_primitive_then(utility::overloaded{
    [&](apply const& a) -> any_expression {
      return apply{ deepened_expression{ ignored_args, bound_depth, a.f } , deepened_expression{ ignored_args, bound_depth, a.x } };
    },
    [&](lambda const& l) -> any_expression {
      return lambda{ deepened_expression{ ignored_args, bound_depth + 1, l.body } };
    },
    [&](argument const& a) -> any_expression {
      if(a.arg < bound_depth) return a;
      else return argument { a.arg + ignored_args };
    },
    [&](axiom const& a) -> any_expression {
      return a;
    }
  });
}
any_expression substitute_expression::reduce() const {
  return target.reduce_to_primitive_then(utility::overloaded{
    [&](apply const& a) -> any_expression {
      return apply{ substitute_expression{ depth, arg, a.f } , substitute_expression{ depth, arg, a.x } };
    },
    [&](lambda const& l) -> any_expression {
      return lambda{ substitute_expression{ depth + 1, arg, l.body } };
    },
    [&](argument const& a) -> any_expression {
      if(a.arg < depth) return a;
      else if(a.arg > depth) return argument{ a.arg - 1 };
      else if(depth == 0) {
        return arg;
      } else {
        return deepened_expression{ depth, 0, arg };
      }
    },
    [&](axiom const& a) -> any_expression {
      return a;
    }
  });
}
namespace {
  void output_expr(std::ostream& o, any_expression const& e) {
    e.reduce_to_primitive_then(utility::overloaded{
      [&](apply const& a) {
        o << "apply("; output_expr(o, a.f); o << ", "; output_expr(o, a.x); o << ")";
      },
      [&](lambda const& l) {
        o << "lambda("; output_expr(o, l.body); o << ")";
      },
      [&](argument const& a) {
        o << "arg(" << a.arg << ")";
      },
      [&](axiom const& a) {
        o << "axiom(" << a.index << ")";
      }
    });
  }
}
std::ostream& operator<<(std::ostream& o, any_expression const& e) {
  output_expr(o, e);
  return o;
}
any_expression totally_simplify(any_expression const& e) {
  return e.reduce_to_primitive_then(utility::overloaded{
    [&](apply const& a) -> any_expression {
      return totally_simplify(a.f).reduce_to_primitive_then(utility::overloaded{
        [&](apply const& aa) -> any_expression {
          return apply{aa, totally_simplify(a.x)};
        },
        [&](lambda const& l) -> any_expression {
          return totally_simplify(substitute_expression{0, a.x, l.body});
        },
        [&](argument const& aa) -> any_expression {
          return apply{aa, totally_simplify(a.x)};
        },
        [&](axiom const& aa) -> any_expression {
          return apply{aa, totally_simplify(a.x)};
        }
      });
    },
    [&](lambda const& l) -> any_expression {
      return lambda{totally_simplify(l.body)};
    },
    [&](argument const& a) -> any_expression {
      return a;
    },
    [&](axiom const& a) -> any_expression {
      return a;
    }
  });
}

bool deep_compare(any_expression lhs, any_expression rhs) {
  return lhs.normalize_locally_then([&]<class T1>(T1 lhs_result) {
    return rhs.normalize_locally_then([&]<class T2>(T2 rhs_result) {
      if constexpr(!std::is_same_v<T1,T2>) return false;
      else if constexpr(std::is_same_v<T1, lambda>) {
        return deep_compare(lhs_result.body, rhs_result.body);
      } else if constexpr(std::is_same_v<T1, normalized_to_arg_t>) {
        return lhs_result.a.arg == rhs_result.a.arg && std::ranges::equal(lhs_result.rev_args, rhs_result.rev_args, &deep_compare);
      } else if constexpr(std::is_same_v<T1, normalized_to_axiom_t>) {
        return lhs_result.a.index == rhs_result.a.index && std::ranges::equal(lhs_result.rev_args, rhs_result.rev_args, &deep_compare);
      }
    });
  });
}
