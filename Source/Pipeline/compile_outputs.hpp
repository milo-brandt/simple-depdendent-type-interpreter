#ifndef COMPILE_PIPELINE_OUTPUTS_HPP
#define COMPILE_PIPELINE_OUTPUTS_HPP

#include "../Compiler/new_instructions.hpp"
#include "../Solver/evaluator.hpp"
#include "../ExpressionParser/expression_generator.hpp"
#include "../Solver/manager.hpp"
#include "../User/debug_format.hpp"

namespace pipeline::compile {
  struct SourceInfo {
    new_expression::Arena& arena;
    std::string_view source;
  };
  struct LexInfo : SourceInfo {
    expression_parser::lex_output::archive_root::Term lexer_output;
    expression_parser::lex_locator::archive_root::Term lexer_locator;
  };
  struct ParseInfo : LexInfo {
    expression_parser::output::archive_root::Expression parser_output;
    expression_parser::locator::archive_root::Expression parser_locator;
  };
  struct ResolveInfo : ParseInfo {
    std::vector<new_expression::TypedValue> embeds;
    expression_parser::resolved::archive_root::Expression parser_resolved;
  };
  struct EvaluateInfo : ResolveInfo {
    new_expression::TypedValue result;
    new_expression::WeakKeyMap<solver::evaluator::variable_explanation::Any> variable_explanations;
    compiler::new_instruction::output::archive_root::Program instruction_output;
    compiler::new_instruction::locator::archive_root::Program instruction_locator;
    std::vector<solver::evaluator::error::Any> evaluation_errors;

    std::string_view locate(compiler::new_instruction::archive_index::PolymorphicKind) const;
    std::optional<std::string_view> get_explicit_name_of(new_expression::WeakExpression primitive) const;
    void report_errors_to(std::ostream&, new_expression::WeakKeyMap<std::string> const& extra_names) const;
    bool is_okay() const;
  };
  void destroy_from_arena(new_expression::Arena&, ResolveInfo&);
  void destroy_from_arena(new_expression::Arena&, EvaluateInfo&);
}

#endif
