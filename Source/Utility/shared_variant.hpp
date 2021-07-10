#include <variant>
#include <memory>

namespace mdb {
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
}
