#include "compile_stages.hpp"
#include <sstream>
#include "../CLI/format.hpp"
#include "../Utility/vector_utility.hpp"

namespace pipeline::compile {
  mdb::Result<LexInfo, std::string> lex(SourceInfo input) {
    expression_parser::LexerInfo lexer_info {
      .symbol_map = {
        {"block", 0},
        {"declare", 1},
        {"axiom", 2},
        {"rule", 3},
        {"let", 4},
        {"where", 14},
        {"match", 15},
        {"verify", 16},
        {"require", 17},
        {"import", 18},
        {"*", 19},
        {"from", 20},
        {"->", 5},
        {":", 6},
        {";", 7},
        {"=", 8},
        {"\\", 9},
        {"\\\\", 10},
        {".", 11},
        {"_", 12},
        {",", 13}
      }
    };
    auto ret = expression_parser::lex_string(input.source, lexer_info);
    if(auto* success = ret.get_if_value()) {
      return LexInfo{
        input,
        archive(std::move(success->output)),
        archive(std::move(success->locator))
      };
    } else {
      auto const& error = ret.get_error();
      std::stringstream err;
      err << "Error: " << error.message << "\nAt " << format_error(input.source.substr(error.position.begin() - input.source.begin()), input.source);
      return err.str();
    }
  }
  mdb::Result<ParseInfo, std::string> parse(LexInfo input) {
    auto ret = expression_parser::parse_lexed(input.lexer_output, [](std::uint64_t symbol) -> expression_parser::ExpressionSymbol {
      switch(symbol) {
        case 0: return expression_parser::ExpressionSymbol::block;
        case 1: return expression_parser::ExpressionSymbol::declare;
        case 2: return expression_parser::ExpressionSymbol::axiom;
        case 3: return expression_parser::ExpressionSymbol::rule;
        case 4: return expression_parser::ExpressionSymbol::let;
        case 5: return expression_parser::ExpressionSymbol::arrow;
        case 6: return expression_parser::ExpressionSymbol::colon;
        case 7: return expression_parser::ExpressionSymbol::semicolon;
        case 8: return expression_parser::ExpressionSymbol::equals;
        case 9: return expression_parser::ExpressionSymbol::backslash;
        case 10: return expression_parser::ExpressionSymbol::double_backslash;
        case 11: return expression_parser::ExpressionSymbol::dot;
        case 12: return expression_parser::ExpressionSymbol::underscore;
        case 13: return expression_parser::ExpressionSymbol::comma;
        case 14: return expression_parser::ExpressionSymbol::where;
        case 15: return expression_parser::ExpressionSymbol::match;
        case 16: return expression_parser::ExpressionSymbol::verify;
        case 17: return expression_parser::ExpressionSymbol::require;
        default: return expression_parser::ExpressionSymbol::unknown;
      }
    });
    if(auto* success = ret.get_if_value()) {
      return ParseInfo{
        std::move(input),
        archive(std::move(success->output)),
        archive(std::move(success->locator))
      };
    } else {
      auto const& error = ret.get_error();
      std::stringstream err;
      err << "Error: " << error.message << "\nAt " << format_error(expression_parser::position_of(input.lexer_locator[error.position]), input.source);
      return err.str();
    }
  }
  mdb::Result<std::pair<expression_parser::ModuleHeader, ParseInfo>, std::string> parse_module(LexInfo input) {
    auto module_parse_result = expression_parser::parse_module_header(input.lexer_output, [](std::uint64_t symbol) -> expression_parser::HeaderSymbol {
      switch(symbol) {
        case 7: return expression_parser::HeaderSymbol::semicolon;
        case 13: return expression_parser::HeaderSymbol::comma;
        case 18: return expression_parser::HeaderSymbol::import;
        case 19: return expression_parser::HeaderSymbol::star;
        case 20: return expression_parser::HeaderSymbol::from;
        default: return expression_parser::HeaderSymbol::unknown;
      }
    });
    if(auto* module_success = module_parse_result.get_if_value()) {
      auto ret = expression_parser::parse_lexed(input.lexer_output, module_success->remaining, [](std::uint64_t symbol) -> expression_parser::ExpressionSymbol {
        switch(symbol) {
          case 0: return expression_parser::ExpressionSymbol::block;
          case 1: return expression_parser::ExpressionSymbol::declare;
          case 2: return expression_parser::ExpressionSymbol::axiom;
          case 3: return expression_parser::ExpressionSymbol::rule;
          case 4: return expression_parser::ExpressionSymbol::let;
          case 5: return expression_parser::ExpressionSymbol::arrow;
          case 6: return expression_parser::ExpressionSymbol::colon;
          case 7: return expression_parser::ExpressionSymbol::semicolon;
          case 8: return expression_parser::ExpressionSymbol::equals;
          case 9: return expression_parser::ExpressionSymbol::backslash;
          case 10: return expression_parser::ExpressionSymbol::double_backslash;
          case 11: return expression_parser::ExpressionSymbol::dot;
          case 12: return expression_parser::ExpressionSymbol::underscore;
          case 13: return expression_parser::ExpressionSymbol::comma;
          case 14: return expression_parser::ExpressionSymbol::where;
          case 15: return expression_parser::ExpressionSymbol::match;
          case 16: return expression_parser::ExpressionSymbol::verify;
          case 17: return expression_parser::ExpressionSymbol::require;
          default: return expression_parser::ExpressionSymbol::unknown;
        }
      });
      if(auto* success = ret.get_if_value()) {
        return std::make_pair(
          std::move(module_success->header),
          ParseInfo{
            std::move(input),
            archive(std::move(success->output)),
            archive(std::move(success->locator))
          }
        );
      } else {
        auto const& error = ret.get_error();
        std::stringstream err;
        err << "Error: " << error.message << "\nAt " << format_error(expression_parser::position_of(input.lexer_locator[error.position]), input.source);
        return err.str();
      }
    } else {
      auto const& error = module_parse_result.get_error();
      std::stringstream err;
      err << "Error: " << error.message << "\nAt " << format_error(expression_parser::position_of(input.lexer_locator[error.position]), input.source);
      return err.str();
    }

  }
  mdb::Result<ResolveInfo, std::string> resolve(ParseInfo input, ResolutionContext context) {
    auto embeds = mdb::make_vector<new_expression::TypedValue>(
      std::move(context.empty_vec),
      std::move(context.cons_vec)
    );
    std::unordered_map<std::string, std::uint64_t> names_to_embeds;
    auto resolved = expression_parser::resolve(expression_parser::resolved::ContextLambda {
      [&](std::string_view str) -> std::optional<std::uint64_t> { //lookup
        std::string s{str};
        if(names_to_embeds.contains(s)) {
          return names_to_embeds.at(s);
        } else if(context.names_to_values.contains(s)) {
          auto index = embeds.size();
          embeds.push_back(copy_on_arena(input.arena, context.names_to_values.at(s)));
          names_to_embeds.insert(std::make_pair(std::move(s), index));
          return index;
        } /*else if(s.starts_with("ext_")) {
          std::stringstream outstr(s.substr(4));
          std::uint64_t ext_index;
          outstr >> ext_index;
          if(ext_index < expression_context.external_info.size()) {
            auto index = embeds.size();
            embeds.push_back(expression_context.get_external(ext_index));
            return index;
          }
        }*/
        return std::nullopt;

      },
      [&](auto const& literal) -> std::uint64_t { //embed_literal
        auto ret = embeds.size();
        std::visit(mdb::overloaded{
          [&](std::uint64_t literal) {
            embeds.push_back({
              .value = context.u64->make_expression(literal),
              .type = input.arena.copy(context.u64->type)
            });
          },
          [&](std::string literal) {
            embeds.push_back({
              .value = context.str->make_expression(primitive::StringHolder{literal}),
              .type = input.arena.copy(context.str->type)
            });
          }
        }, literal);
        return ret;
      }
    }, input.parser_output.root());
    if(auto* resolve = resolved.get_if_value()) {
      return ResolveInfo{
        std::move(input),
        std::move(embeds),
        archive(std::move(*resolve))
      };
    } else {
      std::stringstream err_out;
      auto const& err = resolved.get_error();
      for(auto const& bad_id : err.bad_ids) {
        auto bad_pos = input.parser_locator[bad_id].position;
        err_out << "Bad id: " << format_error(expression_parser::position_of(bad_pos, input.lexer_locator), input.source) << "\n";
      }
      for(auto const& bad_id : err.bad_pattern_ids) {
        auto bad_pos = input.parser_locator[bad_id].position;
        err_out << "Bad pattern id: " << format_error(expression_parser::position_of(bad_pos, input.lexer_locator), input.source) << "\n";
      }
      destroy_from_arena(input.arena, embeds);
      return err_out.str();
    }
  }
  EvaluateInfo evaluate(ResolveInfo input, EvaluationContext context) {
    auto instructions = compiler::new_instruction::make_instructions(input.parser_resolved.root(), {
      .empty_vec_embed_index = 0,
      .cons_vec_embed_index = 1
    });
    auto instruction_output = archive(std::move(instructions.output));
    auto instruction_locator = archive(std::move(instructions.locator));
    solver::Manager manager(context.context);
    new_expression::WeakKeyMap<solver::evaluator::variable_explanation::Any> variable_explanations(input.arena);
    std::vector<solver::evaluator::error::Any> evaluation_errors;
    auto interface = manager.get_evaluator_interface({
      .explain_variable = [&](new_expression::WeakExpression primitive, solver::evaluator::variable_explanation::Any explanation) {
        variable_explanations.set(primitive, explanation);
      },
      .embed = [&](std::uint64_t embed_index) {
        return copy_on_arena(input.arena, input.embeds[embed_index]);
      },
      .report_error = [&](solver::evaluator::error::Any error) {
        evaluation_errors.push_back(std::move(error));
      }
    });
    auto eval_result = solver::evaluator::evaluate(instruction_output.root().get_program_root(), std::move(interface));
    manager.run();
    manager.close();

    auto ret = EvaluateInfo{
      std::move(input),
      std::move(eval_result),
      std::move(variable_explanations),
      std::move(instruction_output),
      std::move(instruction_locator),
      std::move(evaluation_errors)
    };
    return std::move(ret);
  }
  mdb::Result<EvaluateInfo, std::string> full_compile(new_expression::Arena& arena, std::string_view source, CombinedContext context) {
    bool typed_values_used = false;
    auto ret = map(
      bind(
        lex({arena, source}),
        [&](auto lexed) {
          return parse(std::move(lexed));
        },
        [&](auto parsed) {
          typed_values_used = true;
          return resolve(std::move(parsed), {
            .names_to_values = context.names_to_values,
            .u64 = context.u64,
            .str = context.str,
            .empty_vec = std::move(context.empty_vec),
            .cons_vec = std::move(context.cons_vec)
          });
        }
      ),
      [&](auto resolved) {
        return evaluate(std::move(resolved), {
          .context = context.context
        });
      }
    );
    if(!typed_values_used) {
      destroy_from_arena(arena, context.empty_vec, context.cons_vec);
    }
    return ret;
  }
  mdb::Result<std::pair<expression_parser::ModuleHeader, EvaluateInfo>, std::string> full_compile_module(new_expression::Arena& arena, std::string_view source, CombinedContext context) {
    bool typed_values_used = false;
    auto ret = bind(
      lex({arena, source}),
      [&](auto lexed) {
        return parse_module(std::move(lexed));
      },
      [&](auto parsed) {
        typed_values_used = true;
        return map(
          resolve(std::move(parsed.second), {
            .names_to_values = context.names_to_values,
            .u64 = context.u64,
            .str = context.str,
            .empty_vec = std::move(context.empty_vec),
            .cons_vec = std::move(context.cons_vec)
          }),
          [&](auto resolved) {
            return std::make_pair(std::move(parsed.first), evaluate(std::move(resolved), {
              .context = context.context
            }));
          }
        );
      }
    );
    if(!typed_values_used) {
      destroy_from_arena(arena, context.empty_vec, context.cons_vec);
    }
    return ret;

  }

}
