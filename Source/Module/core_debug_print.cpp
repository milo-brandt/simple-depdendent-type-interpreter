#include "core_debug_print.hpp"
#include "../Utility/overloaded.hpp"

namespace expr_module {
  std::ostream& operator<<(std::ostream& o, DebugCoreFormat format) {
    auto const& core = format.core;
    o << "{\n"
    "  .value_import_size = " << core.value_import_size << ",\n"
    "  .data_type_import_size = " << core.data_type_import_size << ",\n"
    "  .c_replacement_import_size = " << core.c_replacement_import_size << ",\n"
    "  .register_count = " << core.register_count << ",\n"
    "  .output_size = " << core.output_size << ",\n"
    "  .steps = {";
    bool first = true;
    for(auto const& step : core.steps) {
      if(first) first = false;
      else o << ",";
      o << "\n    ";
      std::visit(mdb::overloaded{
        [&](step::Embed const& embed) {
          o <<  "Embed{" << embed.import_index << ", " << embed.output << "}";
        },
        [&](step::Apply const& apply) {
          o << "Apply{" << apply.lhs << ", " << apply.rhs << ", " << apply.output << "}";
        },
        [&](step::Declare const& declare) {
          o << "Declare{" << declare.output << "}";
        },
        [&](step::Axiom const& axiom) {
          o << "Axiom{" << axiom.output << "}";
        },
        [&](step::RegisterType const& register_type) {
          o << "RegisterType{" << register_type.index << ", " << register_type.type << "}";
        },
        [&](step::Argument const& argument) {
          o << "Argument{" << argument.index << ", " << argument.output << "}";
        },
        [&](step::Export const& export_expr) {
          o << "Export{" << export_expr.value << "}";
        },
        [&](step::Clear const& clear) {
          o << "Clear{" << clear.target << "}";
        },
        [&](step::Rule const& rule) {
          o << "Rule{\n"
          "      .head = " << rule.head << ",\n"
          "      .args_captured = " << rule.args_captured << ",\n"
          "      .steps = {";
          bool inner_first = true;
          for(auto const& rule_step : rule.steps) {
            if(inner_first) inner_first = false;
            else o << ",";
            o << "\n        ";
            std::visit(mdb::overloaded{
              [&](rule_step::PatternMatch const& match) {
                o << "PatternMatch{" << match.substitution << ", " << match.expected_head << ", " << match.args_captured << "}";
              },
              [&](rule_step::DataCheck const& data_check) {
                o << "DataCheck{" << data_check.capture_index << ", " << data_check.data_type_index << "}";
              },
              [&](rule_step::PullArgument const&) {
                o << "PullArgument{}";
              }
            }, rule_step);
          }
          o << "\n      },\n"
          "      .replacement = ";
          std::visit(mdb::overloaded{
            [&](rule_replacement::Substitution const& substitution) {
              o << "Substitution{" << substitution.substitution << "}";
            },
            [&](rule_replacement::Function const& function) {
              o << "Function{" << function.function_index << "}";
            }
          }, rule.replacement);
          o << "\n    }";
        }
      }, step);
    }
    o << "\n  }\n}";
    return o;
  }

}
