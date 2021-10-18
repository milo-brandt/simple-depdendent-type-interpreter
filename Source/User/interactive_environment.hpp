#ifndef EXPRESSION_NEW_INTERACTIVE_ENVIRONMENT_HPP
#define EXPRESSION_NEW_INTERACTIVE_ENVIRONMENT_HPP

#include <string>
#include <memory>
#include <iostream>

namespace interactive {
  class Environment {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Environment();
    Environment(Environment&&);
    Environment& operator=(Environment&&);
    ~Environment();

    void debug_parse(std::string_view, std::ostream& output = std::cout);
  };
}

#endif
