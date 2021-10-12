#include <type_traits>
#include <tuple>
#include <variant>

namespace mdb {
  namespace parts {
    template<std::size_t> struct Simple{};
    template<std::size_t i> constexpr Simple<i> simple;
    template<class Callback> struct ApplyCall { Callback callback; };
    template<class Callback> constexpr ApplyCall<Callback> apply_call(Callback callback) {
      return {.callback = std::move(callback)};
    }

    namespace detail {
      template<class T, class Callback>
      void visit_children_tuple(T& value, Callback&& callback) {
        constexpr auto size = std::tuple_size_v<std::decay_t<T> >;
        [&]<std::size_t... i>(std::index_sequence<i...>) {
          (callback(std::get<i>(value)) , ...);
        }(std::make_index_sequence<size>{});
      }
      template<class T, class Callback> requires requires(T& value){ value.begin(); value.end(); }
      void visit_children_range(T& value, Callback&& callback) {
        for(auto&& v : value) {
          callback(v);
        }
      }
      template<class... Ts, class Callback>
      void visit_children_variant(std::variant<Ts...>& value, Callback&& callback) {
        return std::visit(std::forward<Callback>(callback), value);
      }
      template<class T, class Callback, class ApplyCallback>
      void visit_children_by(T& value, Callback&& callback, ApplyCall<ApplyCallback> const& apply) {
        apply.callback(value, [&callback](auto&&... args) {
          (callback(args) , ...);
        });
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<0>) {}
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<1>) {
        auto&& [a] = value;
        callback(a);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<2>) {
        auto&& [a, b] = value;
        callback(a); callback(b);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<3>) {
        auto&& [a, b, c] = value;
        callback(a); callback(b); callback(c);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<4>) {
        auto&& [a, b, c, d] = value;
        callback(a); callback(b); callback(c); callback(d);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<5>) {
        auto&& [a, b, c, d, e] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e);
      }
      template<class T>
      static constexpr bool destructures_into_two = requires{ [](T& value) { auto&& [x] = value; }; };
      template<class T, class Callback>
      auto visit_children(T& value, Callback&& callback) {
        if constexpr(requires{ std::decay_t<T>::part_info; }) {
          detail::visit_children_by(value, callback, std::decay_t<T>::part_info); return std::bool_constant<true>{};
        } else if constexpr(requires{ std::tuple_size<std::decay_t<T> >::value; }) {
          detail::visit_children_tuple(value, callback); return std::bool_constant<true>{};
        } else if constexpr(requires{ std::variant_size<std::decay_t<T> >::value; }) {
          detail::visit_children_variant(value, callback); return std::bool_constant<true>{};
        } else if constexpr(requires{ value.begin(); value.end(); }) {
          detail::visit_children_range(value, callback); return std::bool_constant<true>{};
        } else {
          return std::bool_constant<false>{};
        }
      }
    }
    template<class T, class Callback>
    auto visit_children(T&& value, Callback&& callback) {
      static_assert(decltype(detail::visit_children(value, callback))::value, "Type must be possible to visit");
      detail::visit_children(value, callback);
    }
    template<class T>
    constexpr bool visitable = decltype(detail::visit_children(std::declval<T&>(), [](auto&&){}))::value;
  }
}