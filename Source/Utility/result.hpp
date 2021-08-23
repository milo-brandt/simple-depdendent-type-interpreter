#ifndef MDB_RESULT_HPP
#define MDB_RESULT_HPP

#include <variant>

namespace mdb {
  struct InPlaceError{};
  constexpr InPlaceError in_place_error;
  struct InPlaceValue{};
  constexpr InPlaceValue in_place_value;

  template<class V, class E>
  class Result {
    std::variant<V, E> data;
  public:
    using ValueType = V;
    using ErrorType = E;
    Result(V value):data(std::in_place_index<0>, std::move(value)) {}
    Result(E error):data(std::in_place_index<1>, std::move(error)) {}
    template<class... Args>
    Result(InPlaceValue, Args&&... args):data(std::in_place_index<0>, std::forward<Args>(args)...) {}
    template<class... Args>
    Result(InPlaceError, Args&&... args):data(std::in_place_index<1>, std::forward<Args>(args)...) {}

    bool holds_success() const { return data.index() == 0; }
    bool holds_error() const { return data.index() == 1; }

    V* get_if_value() { return std::get_if<0>(&data); }
    V const* get_if_value() const { return std::get_if<0>(&data); }
    E* get_if_error() { return std::get_if<1>(&data); }
    E const* get_if_error() const { return std::get_if<1>(&data); }

    V& get_value() & { return std::get<0>(data); }
    V const& get_value() const& { return std::get<0>(data); }
    V&& get_value() && { return std::move(std::get<0>(data)); }
    E& get_error() & { return std::get<1>(data); }
    E const& get_error() const& { return std::get<1>(data); }
    E&& get_error() && { return std::move(std::get<1>(data)); }

    template<class Value, class Error>
    decltype(auto) visit(Value&& if_value, Error&& if_error) & {
      if(data.index() == 0) {
        return if_value(get_value());
      } else {
        return if_error(get_error());
      }
    }
    template<class Value, class Error>
    decltype(auto) visit(Value&& if_value, Error&& if_error) const& {
      if(data.index() == 0) {
        return if_value(get_value());
      } else {
        return if_error(get_error());
      }
    }
    template<class Value, class Error>
    decltype(auto) visit(Value&& if_value, Error&& if_error) && {
      if(data.index() == 0) {
        return if_value(std::move(get_value()));
      } else {
        return if_error(std::move(get_error()));
      }
    }
  };
  template<class Callback, class V, class E, class OutV = std::invoke_result_t<Callback&&, V&>::ValueType>
  Result<OutV, E> bind(Result<V, E>& result, Callback&& callback) {
    static_assert(std::is_same_v<std::invoke_result_t<Callback&&, V&>::ErrorType, E>, "Error type of bind result must be same as input error type.");
    if(auto* value = result.get_if_value()) {
      return callback(*value);
    } else {
      return {in_place_error, std::move(result.get_error())};
    }
  }
  template<class Callback, class V, class E, class OutV = std::invoke_result_t<Callback&&, V const&>::ValueType>
  Result<OutV, E> bind(Result<V, E> const& result, Callback&& callback) {
    static_assert(std::is_same_v<std::invoke_result_t<Callback&&, V const&>::ErrorType, E>, "Error type of bind result must be same as input error type.");
    if(auto* value = result.get_if_value()) {
      return callback(*value);
    } else {
      return {in_place_error, std::move(result.get_error())};
    }
  }
  template<class Callback, class V, class E, class OutV = std::invoke_result_t<Callback&&, V&&> >
  Result<OutV, E> bind(Result<V, E>&& result, Callback&& callback) {
    static_assert(std::is_same_v<std::invoke_result_t<Callback&&, V&&>::ErrorType, E>, "Error type of bind result must be same as input error type.");
    if(auto* value = result.get_if_value()) {
      return callback(std::move(*value));
    } else {
      return {in_place_error, std::move(result.get_error())};
    }
  }
  template<class V, class E, class Callback1, class Callback2, class... Callbacks>
  decltype(auto) bind(Result<V, E>& start, Callback1&& c1, Callback2&& c2, Callbacks&&... callbacks) {
    return bind(bind(start, std::forward<Callback1>(c1)), std::forward<Callback2>(c2), std::forward<Callbacks>(callbacks)...);
  }
  template<class V, class E, class Callback1, class Callback2, class... Callbacks>
  decltype(auto) bind(Result<V, E> const& start, Callback1&& c1, Callback2&& c2, Callbacks&&... callbacks) {
    return bind(bind(start, std::forward<Callback1>(c1)), std::forward<Callback2>(c2), std::forward<Callbacks>(callbacks)...);
  }
  template<class V, class E, class Callback1, class Callback2, class... Callbacks>
  decltype(auto) bind(Result<V, E>&& start, Callback1&& c1, Callback2&& c2, Callbacks&&... callbacks) {
    return bind(bind(std::move(start), std::forward<Callback1>(c1)), std::forward<Callback2>(c2), std::forward<Callbacks>(callbacks)...);
  }

  template<class Callback, class V, class E, class OutV = std::invoke_result_t<Callback&&, V&> >
  Result<OutV, E> map(Result<V, E>& result, Callback&& callback) {
    if(auto* value = result.get_if_value()) {
      return {in_place_value, callback(*value)};
    } else {
      return {in_place_error, std::move(result.get_error())};
    }
  }
  template<class Callback, class V, class E, class OutV = std::invoke_result_t<Callback&&, V const&> >
  Result<OutV, E> map(Result<V, E> const& result, Callback&& callback) {
    if(auto* value = result.get_if_value()) {
      return {in_place_value, callback(*value)};
    } else {
      return {in_place_error, std::move(result.get_error())};
    }
  }
  template<class Callback, class V, class E, class OutV = std::invoke_result_t<Callback&&, V&&> >
  Result<OutV, E> map(Result<V, E>&& result, Callback&& callback) {
    if(auto* value = result.get_if_value()) {
      return {in_place_value, callback(std::move(*value))};
    } else {
      return {in_place_error, std::move(result.get_error())};
    }
  }
  namespace detail {
    template<class V, class E, class Finisher>
    Result<V, E> gather_map_impl(Finisher&& finisher) {
      return finisher();
    }
    template<class V, class E, class Finisher, class Getter, class... Getters>
    Result<V, E> gather_map_impl(Finisher&& finisher, Getter&& getter, Getters&&... getters) {
      decltype(auto) v = getter();
      if(v.holds_success()) {
        return gather_map_impl<V, E>([&]<class... Args>(Args&&... args) {
          return finisher(std::forward<decltype(v)>(v).get_value(), std::forward<Args>(args)...);
        }, std::forward<Getters>(getters)...);
      } else {
        return {in_place_error, std::forward<decltype(v)>(v).get_error()};
      }
    }
  }
  template<class Callback, class... Getters>
  auto gather_map(Callback&& callback, Getters&&... getters) {
    using V = std::invoke_result_t<Callback&&, decltype(getters().get_value())...>;
    using E = std::common_type_t<typename std::invoke_result_t<Getters&&>::ErrorType...>;
    return detail::gather_map_impl<V, E>([&]<class... Args>(Args&&... args) {
      return Result<V, E>{in_place_value, std::forward<Callback>(callback)(std::forward<Args>(args)...)};
    }, std::forward<Getters>(getters)...);
  }
  namespace detail {
    template<class V, class E, class Finisher, class MergeErrors>
    Result<V, E> multi_error_gather_map_impl_ok(MergeErrors& merge_errors, Finisher&& finisher) {
      return finisher();
    }
    template<class V, class E, class MergeErrors, class... Results>
    Result<V, E> multi_error_gather_map_impl_bad(MergeErrors& merge_errors, E error, Results&&... results) {
      ([&] {
        if(results.holds_error()) {
          error = merge_errors(std::move(error), std::forward<Results>(results).get_error());
        }
      } () , ...);
      return {in_place_error, std::move(error)};
    }
    template<class V, class E, class Finisher, class MergeErrors, class TopResult, class... Results>
    Result<V, E> multi_error_gather_map_impl_ok(MergeErrors& merge_errors, Finisher&& finisher, TopResult&& result, Results&&... results) {
      if(result.holds_success()) {
        return multi_error_gather_map_impl_ok<V, E>(merge_errors, [&]<class... Args>(Args&&... args) {
          return finisher(std::forward<TopResult>(result).get_value(), std::forward<Args>(args)...);
        }, std::forward<Results>(results)...);
      } else {
        return multi_error_gather_map_impl_bad<V, E>(merge_errors, std::forward<TopResult>(result).get_error(), std::forward<Results>(results)...);
      }
    }
  }
  template<class Callback, class MergeErrors, class... Results>
  auto multi_error_gather_map(Callback&& callback, MergeErrors&& merge_errors, Results&&... results) {
    using V = std::invoke_result_t<Callback&&, decltype(std::forward<Results>(results).get_value())...>;
    using E = std::common_type_t<typename std::decay_t<Results>::ErrorType...>;
    return detail::multi_error_gather_map_impl_ok<V, E>(merge_errors, [&]<class... Args>(Args&&... args) {
      return Result<V, E>{in_place_value, std::forward<Callback>(callback)(std::forward<Args>(args)...)};
    }, std::forward<Results>(results)...);
  }
}

#endif
