#include "pattern_outline.hpp"
#include <unordered_map>

namespace compiler::pattern {
  namespace {
    enum class ArgCaptureStatus {
      uncaptured,
      seen,
      captured
    };
  }
  std::optional<ConstrainedPattern> from_expression(expression::tree::Expression const& expr, expression::Context const& expression_context, std::unordered_set<std::uint64_t> const& indeterminates) {
    struct Detail {
      expression::Context const& expression_context;
      std::unordered_set<std::uint64_t> const& indeterminates;
      std::vector<Constraint> constraints;
      std::uint64_t capture_point_index = 0;
      std::unordered_map<std::uint64_t, std::uint64_t> args_to_captures;
      std::vector<std::pair<std::uint64_t, expression::tree::Expression const*> > constrained_capture_points;
      std::optional<Pattern> convert(expression::tree::Expression const& expr, bool spine) {
        auto unfolded = expression::unfold_ref(expr);
        if(auto* arg = unfolded.head->get_if_arg()) {
          if(unfolded.args.size() > 0) return std::nullopt; //can't match an arg applied to things.
          if(args_to_captures.contains(arg->arg_index)) {
            constraints.push_back({
              .capture_point = capture_point_index++,
              .equivalent_expression = expression::tree::Arg{args_to_captures.at(arg->arg_index)}
            });
          } else {
            args_to_captures.insert(std::make_pair(arg->arg_index, capture_point_index++));
          }
          return CapturePoint{};
        }
        auto head_index = unfolded.head->get_external().external_index;
        if(indeterminates.contains(head_index)) return std::nullopt; //cannot match indeterminates ever.
        if(spine && expression_context.external_info[head_index].is_axiom) return std::nullopt; //can't have a rule with axiom head.
        if(!spine && !expression_context.external_info[head_index].is_axiom) { //non-matchable
          constrained_capture_points.emplace_back(capture_point_index++, &expr);
          return CapturePoint{};
        }
        // Beyond here: axiom or declaration applied to args appropriately - woo-hoo!
        Segment ret{
          .head = head_index
        };
        for(auto const* arg : unfolded.args) {
          if(auto pat = convert(*arg, false)) {
            ret.args.push_back(std::move(*pat));
          } else {
            return std::nullopt; //:(
          }
        }
        return ret;
      }
      std::optional<expression::tree::Expression> remap_args_to_captures(expression::tree::Expression const& expr) const {
        return expr.visit(mdb::overloaded{
          [&](expression::tree::Apply const& apply) -> std::optional<expression::tree::Expression> {
            if(auto lhs = remap_args_to_captures(apply.lhs)) {
              if(auto rhs = remap_args_to_captures(apply.rhs)) {
                return expression::tree::Apply{std::move(*lhs), std::move(*rhs)};
              }
            }
            return std::nullopt;
          },
          [&](expression::tree::External const& ext) -> std::optional<expression::tree::Expression> {
            return ext;
          },
          [&](expression::tree::Arg const& arg) -> std::optional<expression::tree::Expression> {
            if(args_to_captures.contains(arg.arg_index)) {
              return expression::tree::Arg{args_to_captures.at(arg.arg_index)};
            } else {
              return std::nullopt; //unmatchable arg
            }
          }
        });
      }
      bool discharge_held_constraints() {
        for(auto const& [capture_point, capture_expr] : constrained_capture_points) {
          if(auto constraint = remap_args_to_captures(*capture_expr)) {
            constraints.push_back({
              .capture_point = capture_point,
              .equivalent_expression = std::move(*constraint)
            });
          } else {
            return false;
          }
        }
        return true;
      }
    };
    Detail detail{
      .expression_context = expression_context,
      .indeterminates = indeterminates
    };
    if(auto pat = detail.convert(expr, true)) {
      if(detail.discharge_held_constraints()) {
        return ConstrainedPattern{
          .pattern = std::move(*pat),
          .constraints = std::move(detail.constraints)
        };
      }
    }
    return std::nullopt;
  }
}
