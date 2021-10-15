#include <type_traits>
#include <tuple>
#include <variant>
#include <vector>

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
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<6>) {
        auto&& [a, b, c, d, e, f] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<7>) {
        auto&& [a, b, c, d, e, f, g] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<8>) {
        auto&& [a, b, c, d, e, f, g, h] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<9>) {
        auto&& [a, b, c, d, e, f, g, h, i] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<10>) {
        auto&& [a, b, c, d, e, f, g, h, i, j] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<11>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j); callback(k);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<12>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j); callback(k); callback(l);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<13>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j); callback(k); callback(l); callback(m);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<14>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j); callback(k); callback(l); callback(m); callback(n);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<15>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j); callback(k); callback(l); callback(m); callback(n); callback(o);
      }
      template<class T, class Callback>
      void visit_children_by(T& value, Callback&& callback, Simple<16>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = value;
        callback(a); callback(b); callback(c); callback(d); callback(e); callback(f); callback(g); callback(h); callback(i); callback(j); callback(k); callback(l); callback(m); callback(n); callback(o); callback(p);
      }
      template<class T>
      struct MapSpecial {
        static constexpr bool is_valid = false;
      };
      template<class T>
      struct MapSpecial<std::vector<T> > {
        static constexpr bool is_valid = true;
        template<class Map>
        static std::vector<T> map_children(std::vector<T>& vec, Map&& map) {
          std::vector<T> ret;
          ret.reserve(vec.size());
          for(auto& v : vec) {
            ret.push_back(map(v));
          }
          return ret;
        }
      };
      template<class T>
      struct MapSpecial<std::vector<T> const> {
        static constexpr bool is_valid = true;
        template<class Map>
        static std::vector<T> map_children(std::vector<T> const& vec, Map&& map) {
          std::vector<T> ret;
          ret.reserve(vec.size());
          for(auto const& v : vec) {
            ret.push_back(map(v));
          }
          return ret;
        }
      };
      template<class T, class Map>
      T map_children_tuple(T& value, Map&& map) {
        constexpr auto size = std::tuple_size_v<std::decay_t<T> >;
        [&]<std::size_t... i>(std::index_sequence<i...>) -> T {
          return T{map(std::get<i>(value))...};
        }(std::make_index_sequence<size>{});
      }
      template<class... Ts, class Map>
      std::variant<Ts...> map_children_variant(std::variant<Ts...>& value, Map&& map) {
        return std::visit([&map](auto& v) -> std::variant<Ts...> {
          return map(v);
        }, value);
      }
      template<class... Ts, class Map>
      std::variant<Ts...> map_children_variant(std::variant<Ts...> const& value, Map&& map) {
        return std::visit([&map](auto const& v) -> std::variant<Ts...> {
          return map(v);
        }, value);
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<0>) {
        return T{};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<1>) {
        auto&& [a] = value;
        return T{map(a)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<2>) {
        auto&& [a, b] = value;
        return T{map(a), map(b)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<3>) {
        auto&& [a, b, c] = value;
        return T{map(a), map(b), map(c)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<4>) {
        auto&& [a, b, c, d] = value;
        return T{map(a), map(b), map(c), map(d)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<5>) {
        auto&& [a, b, c, d, e] = value;
        return T{map(a), map(b), map(c), map(d), map(e)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<6>) {
        auto&& [a, b, c, d, e, f] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<7>) {
        auto&& [a, b, c, d, e, f, g] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<8>) {
        auto&& [a, b, c, d, e, f, g, h] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<9>) {
        auto&& [a, b, c, d, e, f, g, h, i] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<10>) {
        auto&& [a, b, c, d, e, f, g, h, i, j] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<11>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j), map(k)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<12>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j), map(k), map(l)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<13>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j), map(k), map(l), map(m)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<14>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j), map(k), map(l), map(m), map(n)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<15>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j), map(k), map(l), map(m), map(n), map(o)};
      }
      template<class T, class Map>
      T map_children_by(T& value, Map&& map, Simple<16>) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = value;
        return T{map(a), map(b), map(c), map(d), map(e), map(f), map(g), map(h), map(i), map(j), map(k), map(l), map(m), map(n), map(o), map(p)};
      }
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
      struct BadMap {};
      template<class T, class Callback>
      T map_children(T& value, Callback&& callback) {
        if constexpr(requires{ std::decay_t<T>::part_info; }) {
          return detail::map_children_by(value, callback, std::decay_t<T>::part_info);
        } else if constexpr(requires{ std::tuple_size<std::decay_t<T> >::value; }) {
          return detail::map_children_tuple(value, callback);
        } else if constexpr(requires{ std::variant_size<std::decay_t<T> >::value; }) {
          return detail::map_children_variant(value, callback);
        } else if constexpr(detail::MapSpecial<T>::is_valid){
          return detail::MapSpecial<T>::map_children(value, std::forward<Callback>(callback));
        } else {
          return BadMap{};
        }
      }
    }
    template<class T, class Callback>
    auto visit_children(T&& value, Callback&& callback) {
      static_assert(decltype(detail::visit_children(value, callback))::value, "Type must be possible to visit");
      detail::visit_children(value, callback);
    }
    template<class Map, class T>
    T map_children(Map&& map, T& value) {
      //static_assert(decltype(detail::visit_children(value, map))::value, "Type must be possible to visit");
      return detail::map_children(value, map);
    }
    template<class T>
    constexpr bool visitable = decltype(detail::visit_children(std::declval<T&>(), [](auto&&){}))::value;
  }
}
