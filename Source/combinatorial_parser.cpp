#include "combinatorial_parser.hpp"
#include <cctype>

parse_result<std::monostate> recognizer::parse(std::string_view str) const {
  if(str.starts_with(what)) {
    str.remove_prefix(what.size());
    return parse_results::success<std::monostate> {
      {},
      str
    };
  } else {
    return {};
  }
}
