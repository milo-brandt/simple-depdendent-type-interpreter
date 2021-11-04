#ifndef ARENA_PLUGIN_HPP
#define ARENA_PLUGIN_HPP

#include "../Pipeline/standard_compiler_context.hpp"
#include "../Utility/vector_utility.hpp"
#include <iostream>
#include <type_traits>
#include "../Utility/function_info.hpp"
#include "../Utility/span_utility.hpp"

namespace plugin {
  struct Ignore{};
  constexpr Ignore ignore{};
  struct ExpressionHandler{};
  constexpr ExpressionHandler expression_handler{};

  namespace detail {
    template<class T>
    struct SelfDestroyerCRTP {
      new_expression::Arena* arena;
      SelfDestroyerCRTP(new_expression::Arena& arena):arena(&arena) {}
      SelfDestroyerCRTP(SelfDestroyerCRTP const&) = delete;
      SelfDestroyerCRTP(SelfDestroyerCRTP&& other):arena(other.arena) { other.arena = nullptr; }
      SelfDestroyerCRTP& operator=(SelfDestroyerCRTP const&) = delete;
      SelfDestroyerCRTP& operator=(SelfDestroyerCRTP&&) = delete;
      ~SelfDestroyerCRTP() {
        if(arena) {
          destroy_from_arena(*arena, *(T*)this);
        }
      }
    };
    template<class Final, class F1, class... Fs>
    decltype(auto) call_getters(Final&& final, F1&& f1, Fs&&... fs) { //f_n(g) should call g(x1, x2, x3) for some set of args.
      if constexpr(sizeof...(Fs) == 0) {
        return f1(std::forward<Final>(final));
      } else {
        return call_getters([&]<class... Args>(Args&&... args){
          return f1([&]<class... F1Args>(F1Args&&... f1_args) {
            return final(std::forward<F1Args>(f1_args)..., std::forward<Args>(args)...);
          });
        }, std::forward<Fs>(fs)...);
      }
    }
    template<class Final>
    decltype(auto) ordered_eval(Final&& final) {
      return final();
    }
    template<class Final, class F1, class... Fs>
    decltype(auto) ordered_eval(Final&& final, F1&& f1, Fs&&... fs) {
      return ordered_eval([&, arg = f1()]<class... Rest>(Rest&&... rest) mutable -> decltype(auto) {
        return final(std::forward<decltype(f1())>(arg), std::forward<Rest>(rest)...);
      }, std::forward<Fs>(fs)...);
    }

    template<class R> requires std::is_same_v<std::decay_t<R>, Ignore>
    auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, Ignore) {
      return [](new_expression::Arena& arena, std::span<new_expression::WeakExpression> const& args, auto&& callback) {
        return callback(ignore);
      };
    };
    template<class R> requires std::is_same_v<R, new_expression::WeakExpression>
    auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, ExpressionHandler) {
      return [capture_index](new_expression::Arena& arena, std::span<new_expression::WeakExpression> const& args, auto&& callback) {
        return callback(args[capture_index]);
      };
    };

    template<class R, class T>
    auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, new_expression::SharedDataTypePointer<T> ptr)
      requires requires{ (R)ptr->read_data(std::declval<new_expression::Data>()); }
    {
      steps.push_back(new_expression::DataCheck{
        .capture_index = capture_index,
        .expected_type = ptr->type_index
      });
      return [capture_index, ptr = std::move(ptr)](new_expression::Arena& arena, std::span<new_expression::WeakExpression> const& args, auto&& callback) -> decltype(auto) {
        return callback(ptr->read_data(arena.get_data(args[capture_index])));
      };
    }
    template<class R, class T>
    auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps, T value)
      requires requires{ value.template add_constraints_and_get_arg_puller<R>(capture_index, steps); }
    {
      return value.template add_constraints_and_get_arg_puller<R>(capture_index, steps);
    }
    template<class R, class T>
    auto get_embedder(new_expression::SharedDataTypePointer<T> ptr)
      requires requires{ ptr->make_expression(std::declval<R>()); }
    {
      return [ptr = std::move(ptr)](new_expression::Arena& arena, R value) {
        return ptr->make_expression(std::move(value));
      };
    };
    template<class R> requires std::is_same_v<std::decay_t<R>, new_expression::WeakExpression>
    auto get_embedder(ExpressionHandler) {
      return [](new_expression::Arena& arena, R value) {
        return arena.copy(value);
      };
    };
    template<class R> requires std::is_same_v<R, new_expression::OwnedExpression>
    auto get_embedder(ExpressionHandler) {
      return [](new_expression::Arena& arena, R value) {
        return std::move(value);
      };
    };
    template<class R> requires std::is_same_v<R, new_expression::OwnedExpression&&>
    auto get_embedder(ExpressionHandler) {
      return [](new_expression::Arena& arena, R value) {
        return std::move(value);
      };
    };
    template<class R> requires std::is_same_v<R, new_expression::OwnedExpression const&>
    auto get_embedder(ExpressionHandler) {
      return [](new_expression::Arena& arena, R value) {
        return arena.copy(value);
      };
    };
    template<class R, class T> auto get_embedder(T value)
      requires requires{ value.template get_embedder<R>(); }
    {
      return value.template get_embedder<R>();
    }
  }
  template<class... Handlers>
  struct RuleBuilder {
    pipeline::compile::StandardCompilerContext* context;
    std::tuple<Handlers...> handlers;
    template<class T, std::size_t index = 0>
    auto pull_argument(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps) {
      if constexpr(index < sizeof...(Handlers)) {
        auto const& handler = std::get<index>(handlers);
        if constexpr(requires{ detail::add_constraints_and_get_arg_puller<T>(capture_index, steps, handler); }) {
          return detail::add_constraints_and_get_arg_puller<T>(capture_index, steps, handler);
        } else {
          return pull_argument<T, index + 1>(capture_index, steps);
        }
      } else {
        static_assert(index < sizeof...(Handlers), "Handler not found for type.");
      }
    }
    template<class T, std::size_t index = 0>
    auto embedder_for() {
      if constexpr(index < sizeof...(Handlers)) {
        auto const& handler = std::get<index>(handlers);
        if constexpr(requires{ detail::get_embedder<T>(handler); }) {
          return detail::get_embedder<T>(handler);
        } else {
          return embedder_for<T, index + 1>();
        }
      } else {
        static_assert(index < sizeof...(Handlers), "Handler not found for type.");
      }
    }

    template<class Callback>
    void operator()(new_expression::WeakExpression head, Callback callback) {
      [&]<class Ret, class... Args>(mdb::FunctionInfo<Ret(Args...)>) {
        std::vector<new_expression::PatternStep> steps;
        std::size_t capture_count = 0;
        auto arg_puller_tuple = detail::ordered_eval([&]<class... ArgPuller>(ArgPuller&&... arg_pullers) {
          return std::make_tuple(std::move(arg_pullers)...);
        }, [&] {
          steps.push_back(new_expression::PullArgument{});
          return pull_argument<Args>(capture_count++, steps);
        }...);
        auto& arena = context->arena;
        context->context.rule_collector.add_rule({
          .pattern = {
            .head = arena.copy(head),
            .body = {
              .args_captured = sizeof...(Args),
              .steps = std::move(steps)
            }
          },
          .replacement = mdb::function<new_expression::OwnedExpression(std::span<new_expression::WeakExpression>)>{
            [capture_count, &context = *context, callback = std::move(callback), arg_puller_tuple = std::move(arg_puller_tuple), embedder = embedder_for<Ret>()](std::span<new_expression::WeakExpression> inputs) -> new_expression::OwnedExpression {
              if(inputs.size() != capture_count) std::terminate();
              return embedder(context.arena, std::apply([&](auto const&... getters) { return detail::call_getters(callback, [&](auto&& callback) { return getters(context.arena, inputs, callback); }...); }, arg_puller_tuple));
            }
          }
        });
      }(mdb::FunctionInfoFor<Callback>());
    }
  };
  struct BoolHandler {
    new_expression::Arena& arena;
    new_expression::WeakExpression yes;
    new_expression::WeakExpression no;
    struct Embedder : detail::SelfDestroyerCRTP<Embedder> {
      new_expression::OwnedExpression yes;
      new_expression::OwnedExpression no;
      static constexpr auto part_info = mdb::parts::apply_call([](auto& v, auto&& callback) { callback(v.yes, v.no); });
      new_expression::OwnedExpression operator()(new_expression::Arena& arena, bool b) const {
        return arena.copy(b ? yes : no);
      }
    };
    template<class R> requires std::is_same_v<std::decay_t<R>, bool>
    auto get_embedder() {
      return Embedder{
        arena,
        arena.copy(yes),
        arena.copy(no)
      };
    }
  };
  template<class Mark>
  struct ExactHandler {
    new_expression::Arena& arena;
    new_expression::WeakExpression witness;
    template<class R> requires std::is_same_v<std::decay_t<R>, Mark>
    auto add_constraints_and_get_arg_puller(std::size_t capture_index, std::vector<new_expression::PatternStep>& steps) {
      steps.push_back(new_expression::PatternMatch{
        .substitution = arena.argument(capture_index),
        .expected_head = arena.copy(witness),
        .args_captured = 0
      });
      return [](new_expression::Arena&, std::span<new_expression::WeakExpression> const&, auto&& callback) {
        return callback(Mark{});
      };
    }
  };

  template<class... Handlers>
  RuleBuilder<Handlers...> get_rule_builder(pipeline::compile::StandardCompilerContext* context, Handlers... handlers) {
    return {
      .context = context,
      .handlers = std::make_tuple(std::move(handlers)...)
    };
  }
}

#endif
