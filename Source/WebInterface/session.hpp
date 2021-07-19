/*

#include <string>
#include <vector>
#include <variant>
#include <memory>


namespace Web {

  namespace Request {
    struct Initialize{};
    struct Simplify {
      std::size_t index;
    };
    using Any = std::variant<Initialize, Simplify>;
  };


  class Session {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Session();
    Session(Session&&);
    Session& operator=(Session&&);
    ~Session();
    combinator::InterfaceOutput respond(Request::Any request);
  };
}

*/
