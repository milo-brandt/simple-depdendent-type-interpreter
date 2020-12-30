#include "standard_expression.hpp"

namespace expressionz::standard {
  primitives::axiom context::register_axiom(axiom_info a) {
    auto ret = axioms.size();
    axioms.push_back(std::move(a));
    return {ret};
  }
  primitives::declaration context::register_declaration(declaration_info d) {
    auto ret = declarations.size();
    declarations.push_back(std::move(d));
    return {ret};
  }
  void context::add_rule(primitives::declaration d, simplification_rule r) {
    declarations[d.index].rules.push_back(std::move(r));
  }
  namespace {
    namespace matching_actions {
      struct store {
        std::size_t match_index;
        std::size_t output_index;
      };
      struct compare {
        std::size_t match_index;
        std::size_t output_index;
      };
      struct expand {
        std::size_t match_index;
        std::size_t expected_arg_count;
        primitives::any expected_head;
      };
      using any = std::variant<store, compare, expand>;
    };
    struct matching_routine_specification {
      std::size_t output_args;
      std::vector<matching_actions::any> actions;
    };
    routine<std::optional<std::vector<expressionz::coro::handles::expression> > > run_matching_routine(std::vector<expressionz::coro::handles::expression> stack, simplify_routine_t const& s, matching_routine_specification const& m) {
      using namespace expressionz::coro;
      std::vector<expressionz::coro::handles::expression> ret;
      ret.resize(m.output_args, handles::expression{-std::size_t(1)});
      for(auto const& action : m.actions) {
        if(auto const* store = std::get_if<matching_actions::store>(&action)) {
          ret[store->output_index] = stack[store->match_index];
        } else if(auto const* store = std::get_if<matching_actions::compare>(&action)) {
          //todo
        } else if(auto const* expand = std::get_if<matching_actions::expand>(&action)) {
          auto r = co_await s(stack[expand->match_index]);
          if(!r) co_return std::nullopt;
          if(r->head != expand->expected_head) co_return std::nullopt;
          if(r->args.size() != expand->expected_arg_count) co_return std::nullopt;
          stack.insert(stack.end(), r->args.begin(), r->args.end());
        }
      }
      co_return ret;
    }
    routine<std::optional<standard_expression> > run_matching_routine_then(std::vector<expressionz::coro::handles::expression> stack, simplify_routine_t const& s, matching_routine_specification const& m, simplification_rule_routine const& simplifier) {
      auto true_args = co_await run_matching_routine(std::move(stack), s, m);
      if(!true_args) co_return std::nullopt;
      co_yield simplifier(s, std::move(*true_args));
      std::terminate(); //co_yield doesn't resume
    }
    struct matching_routine_builder {
      std::size_t stack_size;
      std::vector<bool> used_args;
      std::vector<matching_actions::any> actions;
      bool insert_at_index(std::size_t out_index) {
        if(used_args.size() <= out_index) used_args.resize(out_index + 1, false);
        bool ret = used_args[out_index];
        used_args[out_index] = true;
        return ret;
      }
    };
    routine<std::monostate> extend_matching_routine_definition(matching_routine_builder& ret, std::size_t base_position, expressionz::coro::handles::expression e, context const& ctx) {
      using namespace expressionz::coro;
      auto form = co_await simplify_outer(e, ctx);
      if(form) {
        ret.actions.push_back(matching_actions::expand{base_position, form->args.size(), form->head});
        std::size_t new_base = ret.stack_size;
        ret.stack_size += form->args.size();
        for(auto arg : form->args) {
          co_await extend_matching_routine_definition(ret, new_base++, arg, ctx);
        }
        co_return std::monostate{};
      } else {
        auto exp = co_await actions::expand(e);
        if(auto* arg = std::get_if<handles::argument>(&exp)) {
          if(ret.insert_at_index(arg->index)) {
            ret.actions.push_back(matching_actions::compare{base_position, arg->index});
          } else {
            ret.actions.push_back(matching_actions::store{base_position, arg->index});
          }
          co_return std::monostate{};
        } else {
          throw std::runtime_error("term was neither axiom expression nor argument");
        }
      }
    }
    routine<std::pair<primitives::declaration, simplification_rule> > create_matching_routine(expressionz::coro::handles::expression e, simplification_rule_routine result, context const& ctx) {
      using namespace expressionz::coro;
      co_await simplify_total(e, ctx);
      std::vector<handles::expression> args;
      std::optional<primitives::declaration> declaration;
      handles::expression head = e;
      while(true) {
        auto v = co_await actions::expand(head);
        if(auto* a = std::get_if<handles::apply>(&v)) {
          args.push_back(a->x);
          head = a->f;
        } else if(auto* a = std::get_if<primitives::any>(&v)) {
          if(auto* d = std::get_if<primitives::declaration>(a)) {
            declaration = std::move(*d);
            break;
          } else {
            throw std::runtime_error("head of expression was a primitive, but not a declaration");
          }
        } else {
          throw std::runtime_error("head of expression was not a primitive");
        }
      }
      std::reverse(args.begin(), args.end()); //too lazy to use rbegin and rend later
      matching_routine_builder builder;
      builder.stack_size = args.size();
      std::size_t base_position = 0;
      for(auto arg : args) {
        co_await extend_matching_routine_definition(builder, base_position++, arg, ctx);
      }
      for(auto b : builder.used_args) {
        if(!b) throw std::runtime_error("the arguments appearing in the pattern were not contiguous");
      }
      matching_routine_specification spec{
        .output_args = builder.used_args.size(),
        .actions = std::move(builder.actions)
      };
      co_return std::make_pair(
        std::move(*declaration),
        simplification_rule{
          .args_consumed = args.size(),
          .matcher = [spec = std::move(spec), result = std::move(result)](auto const& s, std::vector<handles::expression> stack) {
            return run_matching_routine_then(std::move(stack), s, spec, result);
          }
        }
      );
    }
    routine<std::optional<standard_expression> > make_replacements(standard_expression replacement, std::vector<expressionz::coro::handles::expression> args, simplify_routine_t const& s) {
      using namespace expressionz::coro;
      std::vector<standard_expression> e_args;
      for(auto arg : args) {
        e_args.push_back(co_await actions::collapse(arg));
      }
      co_return substitute_many(std::move(e_args), replacement);
    }
  }
  std::pair<primitives::declaration, simplification_rule> create_pattern_rule(standard_expression pattern, simplification_rule_routine result, context const& ctx) {
    auto [_, ret] = run_routine(create_matching_routine, pattern, std::move(result), ctx);
    if(!ret) throw std::runtime_error("something went horribly wrong");
    return std::move(*ret);
  }
  std::pair<primitives::declaration, simplification_rule> create_pattern_replacement_rule(standard_expression pattern, standard_expression replacement, context const& ctx) {
    using namespace expressionz::coro;
    return create_pattern_rule(std::move(pattern), [replacement = std::move(replacement)](auto const& s, std::vector<handles::expression> args){
      return make_replacements(replacement, std::move(args), s);
    }, ctx);
  }
  routine<std::optional<simplification_result> > simplify_outer(expressionz::coro::handles::expression e, context const& ctx) {
    using namespace expressionz::coro;
    struct arg_segment {
      handles::expression arg;
      handles::expression base;
    };
    std::vector<arg_segment> args;
    handles::expression head = e;
    while(true) {
      auto v = co_await actions::expand(head);
      if(auto* a = std::get_if<handles::apply>(&v)) {
        args.push_back({a->x, head});
        head = a->f;
      } else if(auto* a = std::get_if<handles::abstract>(&v)) {
        if(args.size() > 0) {
          auto back = std::move(args.back());
          args.pop_back();
          head = std::move(back.base);
          co_await actions::replace<primitives::any>(head, substitute(co_await actions::collapse(std::move(back.arg)), co_await actions::collapse(std::move(a->body))));
        } else {
          co_return std::nullopt;
        }
      } else if(auto* a = std::get_if<primitives::any>(&v)) {
        if(auto* axiom = std::get_if<primitives::axiom>(a)) {
          std::vector<handles::expression> args_fwd;
          std::transform(args.rbegin(), args.rend(), std::back_inserter(args_fwd), [](auto& a){ return std::move(a.arg); });
          co_return simplification_result{*axiom, std::move(args_fwd)};
        } else if(auto* declaration = std::get_if<primitives::declaration>(a)) {
          auto const& rules = ctx.declarations[declaration->index].rules;
          bool success = false;
          for(auto const& rule : rules) {
            if(rule.args_consumed > args.size()) {
              co_return std::nullopt; //not enough arguments
            } else {
              std::vector<handles::expression> args_fwd;
              std::transform(args.rbegin(), args.rbegin() + rule.args_consumed, std::back_inserter(args_fwd), [](auto& a){ return std::move(a.arg); });
              auto new_v = co_await rule.matcher([&](handles::expression e){ return simplify_outer(e, ctx); }, std::move(args_fwd));
              if(new_v) {
                if(rule.args_consumed > 0) head = args[args.size() - rule.args_consumed].base;
                co_await actions::replace<primitives::any>(head, std::move(*new_v));
                args.erase(args.end() - rule.args_consumed, args.end());
                success = true;
                break;
              }
            }
          }
          if(!success) {
            co_return std::nullopt; //no rule recognized
          }
        } else if(auto* i = std::get_if<std::uint64_t>(a)){
          std::vector<handles::expression> args_fwd;
          std::transform(args.rbegin(), args.rend(), std::back_inserter(args_fwd), [](auto& a){ return std::move(a.arg); });
          co_return simplification_result{*i, std::move(args_fwd)};
        } else {
          std::terminate();
        }
      } else {
        //argument!
        co_return std::nullopt;
      }
    }
    std::terminate(); //unreachable
  }
  routine<std::monostate> simplify_total(expressionz::coro::handles::expression e, context const& ctx) {
    using namespace expressionz::coro;
    co_await simplify_outer(e, ctx);
    auto head = e;
    while(true) {
      auto v = co_await actions::expand(head);
      if(auto* a = std::get_if<handles::apply>(&v)) {
        head = a->f;
        co_await simplify_total(a->x, ctx);
      } else if(auto* a = std::get_if<handles::abstract>(&v)) {
        co_await simplify_total(a->body, ctx);
        co_return std::monostate{};
      } else {
        co_return std::monostate{};
      }
    }
  }
  namespace {
    routine<std::monostate> simple_output_impl(expressionz::coro::handles::expression e, context const& ctx, std::ostream& o, std::size_t depth) {
      using namespace expressionz::coro;
      auto v = co_await actions::expand(e);
      if(auto* a = std::get_if<handles::apply>(&v)) {
        o << "(";
        co_await simple_output_impl(a->f, ctx, o, depth);
        o << " ";
        co_await simple_output_impl(a->x, ctx, o, depth);
        o << ")";
      } else if(auto* a = std::get_if<handles::abstract>(&v)) {
        o << "(\\" << (depth + 1) << " => ";
        co_await simple_output_impl(a->body, ctx, o, depth + 1);
        o << ")";
      } else if(auto* a = std::get_if<handles::argument>(&v)) {
        o << "$" << long(depth - a->index);
      } else if(auto* a = std::get_if<primitives::any>(&v)) {
        if(auto* axiom = std::get_if<primitives::axiom>(a)) {
          o << ctx.axioms[axiom->index].name;
        } else if(auto* declaration = std::get_if<primitives::declaration>(a)) {
          o << ctx.declarations[declaration->index].name;
        } else if(auto* i = std::get_if<std::uint64_t>(a)) {
          o << *i;
        } else {
          std::terminate();
        }
      } else {
        std::terminate();
      }
      co_return std::monostate{};
    }
  }
  routine<std::monostate> simple_output(expressionz::coro::handles::expression e, context const& ctx, std::ostream& o) {
    co_yield simple_output_impl(std::move(e), ctx, o, 0);
    std::terminate(); //doesn't resume, but compiler doesn't know
  }

}
