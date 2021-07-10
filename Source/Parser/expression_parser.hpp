#ifndef EXPRESSION_PARSER_HPP
#define EXPRESSION_PARSER_HPP

namespace parser {
  namespace standard {
  MB_DECLARE_RECURSIVE_PARSER(expression)

  MB_DEFINE_RECURSIVE_PARSER(expression, branch(
    std::make_pair(sequence_spaced(open_paren, id_name, colon), dependent_arrow_expression),
    std::make_pair(sequence_spaced(id_name, arrow), arrow_expression),
    std::make_pair(always_match, apply_expression)
  ))
  }
}

#endif
