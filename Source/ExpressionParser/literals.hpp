#ifndef EXPRESSION_LITERALS_HPP
#define EXPRESSION_LITERALS_HPP

#include <variant>
#include <cstdint>
#include <string>

namespace expression_parser::literal {
  using Any = std::variant<std::uint64_t, std::string>;
}

#endif
