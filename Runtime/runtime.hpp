#ifndef runtime_hpp
#define runtime_hpp

#include "block.hpp"

namespace runtime{
  class environment {
    struct _impl;
    std::unique_ptr<_impl> data;
  public:
    using fn_type = std::size_t(*)(std::size_t);
    environment();
    ~environment();
    fn_type compile(block const&, std::string const& name);
  };
};

#endif
