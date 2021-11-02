#ifndef MODULE_HEADER_PARSER_HPP
#define MODULE_HEADER_PARSER_HPP

#include "lexer_tree.hpp"
#include "../Utility/function.hpp"

namespace expression_parser {
  struct ImportInfo {
    std::string module_name;
    bool request_all;
    std::vector<std::string> requested_names;
  };
  struct ModuleHeader {
    std::vector<ImportInfo> imports;
  };
  struct ModuleResult {
    ModuleHeader header;
    LexerSpanIndex remaining;
  };
  enum class HeaderSymbol {
    import, from, comma, semicolon, star, unknown
  };
  using HeaderSymbolMap = mdb::function<HeaderSymbol(std::uint64_t)>;
  mdb::Result<ModuleResult, ParseError> parse_module_header(lex_output::archive_root::Term const&, HeaderSymbolMap);
}

#endif
