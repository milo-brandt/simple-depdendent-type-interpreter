#include "evaluation.hpp"
#include "arena_utility.hpp"
#include <algorithm>
#include "../Utility/vector_utility.hpp"

namespace new_expression {
  namespace {
    struct AxiomReductionRow {
      OwnedExpression reducer;
      OwnedExpression output;
      std::uint64_t mark_index;
    };
    struct ReductionRow {
      std::vector<OwnedExpression> reducers;
      OwnedExpression conglomerate_representative;
      OwnedExpression canonical_form;
      std::uint64_t conglomerate_index;
    };
    struct Table {
      std::vector<AxiomReductionRow> axiom_rows;
      std::vector<ReductionRow> rows;
      WeakKeyMap<std::size_t> representative_to_row;
    };
    template<class Base> //Base must provide Arena& arena and OwnedExpression reduce()
    struct ReducerCRTP {
      Base& me() { return *(Base*)this; }
      bool reduce_by_pattern(OwnedExpression& expr, RuleCollector const& collector) {
        auto& arena = me().arena;
        auto unfolded = unfold(arena, expr);
        if(arena.holds_declaration(unfolded.head)) {
          auto const& declaration_info = collector.declaration_info(unfolded.head);
          for(auto const& rule : declaration_info.rules) {
            if(rule.pattern_body.args_captured <= unfolded.args.size()) {
              std::vector<WeakExpression> pattern_stack;
              pattern_stack.insert(pattern_stack.end(), unfolded.args.begin(), unfolded.args.end());
              std::vector<OwnedExpression> novel_roots; //storage for new expressions we create
              for(auto const& match : rule.pattern_body.sub_matches) {
                auto new_expr = me().reduce(
                  substitute_into(arena, match.substitution, mdb::as_span(pattern_stack))
                );
                auto match_unfold = unfold(arena, new_expr);
                novel_roots.push_back(std::move(new_expr)); //keep reference for later deletion
                if(match_unfold.head != match.expected_head) goto PATTERN_FAILED;
                if(match_unfold.args.size() != match.args_captured) goto PATTERN_FAILED;
                pattern_stack.insert(pattern_stack.end(), match_unfold.args.begin(), match_unfold.args.end());
              }
              for(auto const& data_check : rule.pattern_body.data_checks) {
                auto new_expr = me().reduce(arena.copy(pattern_stack[data_check.capture_index]));
                pattern_stack[data_check.capture_index] = new_expr;
                bool success = [&] {
                  if(auto* data = arena.get_if_data(new_expr)) {
                    return data->type_index == data_check.expected_type;
                  } else {
                    return false;
                  }
                }();
                novel_roots.push_back(std::move(new_expr));
                if(!success) goto PATTERN_FAILED;
              }
              //if we get here, the pattern succeeded.
              {
                auto new_expr = substitute_into(arena, rule.replacement, mdb::as_span(pattern_stack));
                arena.drop(std::move(expr));
                expr = std::move(new_expr);
                destroy_from_arena(arena, novel_roots);
                return true;
              }
            PATTERN_FAILED:
              destroy_from_arena(arena, novel_roots);
            }
          }
        }
        return false;
      }
    };
    struct SimpleReducer : ReducerCRTP<SimpleReducer> {
      Arena& arena;
      RuleCollector const& rule_collector;
      WeakKeyMap<OwnedExpression, PartDestroyer> reductions;
      OwnedExpression reduce(OwnedExpression expr) {
        if(reductions.contains(expr)) {
          auto ret = arena.copy(reductions.at(expr));
          arena.drop(std::move(expr));
          return ret;
        }
        WeakExpression input = expr;
        while(reduce_by_pattern(expr, rule_collector));
        reductions.set(input, arena.copy(expr));
        return std::move(expr);
      }
    };
    /*
    struct MergeWith {
      std::uint64_t other_mark;
    };
    std::variant<OwnedExpression, MergeWith> reduce(Arena& arena, Table& table, OwnedExpression input, std::uint64_t row_simplifying, bool outer_level) {
      //do normal reduction stuff...
      for(std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
        auto& row = table.rows[row_index];
        for(std::size_t reducer_index = 0; reducer_index < row.reducers.size(); ++reducer_index) {
          auto& reducer = row.reducers[reducer_index];
          if(input == reducer) {
            if(outer_level) {
              arena.drop(std::move(input));
              //return row.conglomerate_index;
            }
            auto output = arena.copy(row.conglomerate_representative);
            if(row_index >= row_simplifying) {
              std::rotate( //rotate table so that the used term comes first
                table.rows.begin() + row_simplifying,
                table.rows.begin() + row_index,
                table.rows.begin() + row_index + 1
              );
              std::swap(reducer, row.canonical_form); //the reducer is now the canonical form of its row

            } else {

            }
          }
        }
      }
      for(auto const& row : table) {
        for(auto const& reducer : row.reducers) {

        }
      }
    }*/
  }
  SimpleEvaluationContext::SimpleEvaluationContext(Arena& arena, RuleCollector& rule_collector):arena(&arena), rule_collector(&rule_collector) {}
  OwnedExpression SimpleEvaluationContext::reduce_head(OwnedExpression expr) {
    return SimpleReducer{
      .arena = *arena,
      .rule_collector = *rule_collector,
      .reductions{*arena}
    }.reduce(std::move(expr));
  }

}
