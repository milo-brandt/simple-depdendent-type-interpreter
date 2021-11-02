#ifndef COMPILE_PIPELINE_STAGES_HPP
#define COMPILE_PIPELINE_STAGES_HPP

#include "compile_outputs.hpp"
#include "../Primitives/primitives.hpp"
#include "../ExpressionParser/module_header.hpp"

namespace pipeline::compile {
  mdb::Result<LexInfo, std::string> lex(SourceInfo input);
  mdb::Result<ParseInfo, std::string> parse(LexInfo input);
  mdb::Result<std::pair<expression_parser::ModuleHeader, ParseInfo>, std::string> parse_module(LexInfo input);
  struct ResolutionContext {
    std::unordered_map<std::string, new_expression::TypedValue> const& names_to_values;
    primitive::U64Data* u64;
    primitive::StringData* str;
    new_expression::TypedValue empty_vec;
    new_expression::TypedValue cons_vec;
  };
  mdb::Result<ResolveInfo, std::string> resolve(ParseInfo input, ResolutionContext);
  struct EvaluationContext {
    solver::BasicContext& context;
  };
  EvaluateInfo evaluate(ResolveInfo info, EvaluationContext);
  struct CombinedContext {
    std::unordered_map<std::string, new_expression::TypedValue> const& names_to_values;
    primitive::U64Data* u64;
    primitive::StringData* str;
    new_expression::TypedValue empty_vec;
    new_expression::TypedValue cons_vec;
    solver::BasicContext& context;
  };
  mdb::Result<EvaluateInfo, std::string> full_compile(new_expression::Arena&, std::string_view source, CombinedContext);
  mdb::Result<std::pair<expression_parser::ModuleHeader, EvaluateInfo>, std::string> full_compile_module(new_expression::Arena&, std::string_view source, CombinedContext);
}

#endif
