#include <optional>
#include <atomic>
#include <memory>
#include <variant>
#include "tags.hpp"
#include "function.hpp"

/*
Monadic utilities for building "asynchronous" mechanisms.

Essentially, requires the user to implement various Primitives with a static member is_primitive,
then to implement an Interpreter class with the method
  .request(Primitive primitive, Callback callback)

Note that this would all, ideally, be done with C++20 coroutines instead, but
compiler support for those isn't yet sufficient.
*/
namespace mdb::async {
  template<class T>
  concept is_primitive = requires{ T::is_primitive; };
  template<class T>
  using RoutineTypeOf = typename T::RoutineType;
  namespace detail {
    template<class Callback, class Arg, class... Args>
    decltype(auto) rotate_args(Callback&& callback, Arg&& arg, Args&&... args) { //call with last argument first
      if constexpr(sizeof...(Args) > 0) {
        return rotate_args([&]<class R1, class... R>(R1&& first, R&&... inner) {
          return std::forward<Callback>(callback)(std::forward<R1>(first), std::forward<Arg>(arg), std::forward<R>(inner)...);
        }, std::forward<Args>(args)...);
      } else {
        return callback(std::forward<Arg>(arg));
      }
    }
  }
  template<class RetType, class Base, class Then>
  struct Bind {
    using RoutineType = RetType;
    Base base;
    Then then;
  };
  template<class RetType, class Base, class... Then>
  struct Multibind {
    using RoutineType = RetType;
    Base base;
    std::tuple<Then...> then;
  };
  template<class RetType, class Then, class... Base>
  struct ParallelBind {
    using RoutineType = RetType;
    std::tuple<Base...> base;
    Then then;
  };
  template<class T>
  struct Pure {
    using RoutineType = T;
    T value;
  };
  template<class RetType, class Base, class Then>
  auto bind(Base base, Then then) {
    return Bind<RetType, Base, Then>{
      .base = std::move(base),
      .then = std::move(then)
    };
  }
  template<class RetType, class Base, class Then1, class Then2, class... Then>
  auto bind(Base base, Then1 then1, Then2 then2, Then... then) {
    return Multibind<RetType, Base, Then1, Then2, Then...>{
      .base = std::move(base),
      .then = {std::move(then1), std::move(then2), std::move(then)...}
    };
  }
  template<class RetType, class... Ts>
  auto parallel_bind(Ts&&... values) {
    return detail::rotate_args([&]<class Then, class... Base>(Then then, Base... base) {
      return ParallelBind<RetType, Then, Base...>{
        .base = {std::move(base)...},
        .then = std::move(then)
      };
    }, std::forward<Ts>(values)...);
  }
  template<class T>
  auto pure(T value) {
    return Pure<T>{
      .value = std::forward<T>(value)
    };
  }
  template<class Base, class Map>
  auto map(Base&& base, Map&& map) {
    using BaseType = RoutineTypeOf<std::decay_t<Base> >;
    using RetType = decltype(map(std::declval<BaseType>()));
    return bind<RetType>(std::forward<Base>(base), [map = std::forward<Map>(map)](auto&& ret, BaseType value) mutable {
      ret(pure(map(value)));
    });
  }
  template<class Adaptor, class Body>
  struct Adapt {
    using RoutineType = RoutineTypeOf<Body>;
    Adaptor adaptor; //has .transform method for any requests
    Body body;
  };
  template<class Adaptor, class Body>
  auto adapt(Adaptor adaptor, Body body) {
    return Adapt<Adaptor, Body>{
      .adaptor = std::move(adaptor),
      .body = std::move(body)
    };
  }
  template<class RetType, class F>
  struct Complex {
    using RoutineType = RetType;
    F controller;
  };
  template<class RetType, class F>
  auto complex(F controller) {
    return Complex<RetType, F>{
      .controller = std::move(controller)
    };
  }

  template<class Interpreter, class Primitive, class Then> requires is_primitive<Primitive>
  void execute_then(Interpreter interpreter, Primitive program, Then then) {
    interpreter.request(std::move(program), std::move(then));
  }
  template<class Interpreter, class Primitive, class Then> requires is_primitive<Primitive>
  void execute_then(mdb::Ref<Interpreter> interpreter, Primitive program, Then then) {
    return interpreter.ptr->request(std::move(program), std::move(then));
  }
  template<class Interpreter, class RetType, class BindBase, class BindThen, class Then>
  void execute_then(Interpreter interpreter, Bind<RetType, BindBase, BindThen> program, Then then) {
    execute_then(interpreter, std::move(program.base), [interpreter, then = std::move(then), bind_then = std::move(program.then)]<class BaseRet>(BaseRet&& value) mutable {
      bool called = false;
      auto ret = [&]<class Next>(Next&& next) requires std::is_same_v<RoutineTypeOf<Next>, RetType> {
        if(called) std::terminate(); //must only call ret once!
        called = true;
        execute_then(interpreter, std::forward<Next>(next), std::move(then));
      };
      std::move(bind_then)(ret, std::forward<BaseRet>(value));
      if(!called) std::terminate(); //must call ret!
    });
  }
  namespace detail {
    template<class T, class... Ts>
    auto& get_tuple_head(std::tuple<T, Ts...>& tuple) {
      return std::get<0>(tuple);
    }
    template<class T, class... Ts>
    std::tuple<Ts...> get_tuple_tail(std::tuple<T, Ts...>&& tuple) {
      return std::apply([](auto&&, Ts&&... ts) {
        return std::tuple<Ts...>{std::forward<Ts>(ts)...};
      }, std::move(tuple));
    }
    template<class Interpreter, class Then, class... Ts>
    auto continuation_function(Interpreter interpreter, Then then, std::tuple<Ts...> remaining) {
      if constexpr(sizeof...(Ts) == 0) {
        return std::move(then);
      } else {
        return [interpreter, then = std::move(then), remaining = std::move(remaining)]<class BaseRet>(BaseRet&& value) mutable {
          bool called = false;
          auto tuple_head = std::move(detail::get_tuple_head(remaining));
          auto ret = [&]<class Next>(Next&& next) {
            if(called) std::terminate(); //must only call ret once!
            called = true;
            execute_then(interpreter, std::forward<Next>(next), continuation_function(interpreter, std::move(then), get_tuple_tail(std::move(remaining))));
          };
          std::move(tuple_head)(ret, std::forward<BaseRet>(value));
          if(!called) std::terminate(); //must call ret!
        };
      }
    }
  }
  template<class Interpreter, class RetType, class BindBase, class... BindThen, class Then>
  void execute_then(Interpreter interpreter, Multibind<RetType, BindBase, BindThen...> program, Then then) {
    execute_then(interpreter, std::move(program.base), detail::continuation_function(interpreter, std::move(then), std::move(program.then)));
  }
  namespace detail {
    template<class Then, class... BindBase>
    struct ParallelSharedState {
      std::atomic<std::uint64_t> parts_needed = sizeof...(BindBase);
      std::tuple<std::optional<RoutineTypeOf<BindBase> >...> parts;
      Then then;
      ParallelSharedState(Then then):then(std::move(then)) {}
      template<std::size_t index, class T>
      void set(T&& value) {
        auto& pos = std::get<index>(parts);
        if(pos) std::terminate(); //can't set twice!
        pos = std::forward<T>(value);
        if(--parts_needed == 0) {
          std::apply([&](std::optional<RoutineTypeOf<BindBase> >&&... args) {
            std::move(then)(std::move(*args)...);
          }, std::move(parts));
        }
      }
    };
  }
  template<class Interpreter, class RetType, class BindThen, class... BindBase, class Then>
  void execute_then(Interpreter interpreter, ParallelBind<RetType, BindThen, BindBase...> program, Then then) {
    [&]<std::size_t... index>(std::index_sequence<index...>) {
      auto finish = [interpreter, then = std::move(then), bind_then = std::move(program.then)](RoutineTypeOf<BindBase>&&... values) mutable {
        bool called = false;
        auto ret = [&]<class Next>(Next&& next) requires std::is_same_v<RoutineTypeOf<Next>, RetType> {
          if(called) std::terminate(); //must only call ret once!
          called = true;
          execute_then(interpreter, std::forward<Next>(next), std::move(then));
        };
        std::move(bind_then)(ret, std::move(values)...);
        if(!called) std::terminate(); //must call ret!
      };
      if constexpr(sizeof...(BindBase) == 0) {
        finish();
      } else {
        auto shared_state = std::make_shared<detail::ParallelSharedState<decltype(finish), BindBase...> >(std::move(finish));
        ([&] {
          execute_then(interpreter, std::get<index>(program.base), [shared_state](RoutineTypeOf<BindBase> value) {
            shared_state->template set<index>(std::move(value));
          });
        }() , ...);
      }
    }(std::make_index_sequence<sizeof...(BindBase)>{});
  }

  template<class Interpreter, class T, class Then>
  void execute_then(Interpreter&&, Pure<T> pure, Then then) {
    std::move(then)(std::move(pure.value));
  }
  namespace detail {
    template<class Interpreter, class Adaptor>
    struct AdaptedInterpreter {
      Interpreter base;
      Adaptor adaptor;
      template<class Request, class Then>
      void request(Request&& request, Then&& then) {
        execute_then(
          base,
          adaptor.transform(std::forward<Request>(request)),
          std::forward<Then>(then)
        );
      }
    };
  }
  template<class Interpreter, class Adaptor, class Body, class Then>
  void execute_then(Interpreter interpreter, Adapt<Adaptor, Body> program, Then then) {
    execute_then(detail::AdaptedInterpreter<Interpreter, Adaptor>{
      .base = std::move(interpreter),
      .adaptor = std::move(program.adaptor)
    }, std::move(program.body), std::move(then));
  }
  template<class Interpreter, class RetType, class F, class Then>
  void execute_then(Interpreter interpreter, Complex<RetType, F> complex, Then then) {
    std::move(complex.controller)([interpreter]<class Routine, class InnerThen>(Routine&& routine, InnerThen&& inner_then) {
      execute_then(interpreter, std::forward<Routine>(routine), std::forward<InnerThen>(inner_then));
    }, std::move(then));
  }
  template<class Interpreter, class Program>
  void execute(Interpreter interpreter, Program&& program) {
    execute_then(std::move(interpreter), std::forward<Program>(program), [](auto&&...){});
  }
}
