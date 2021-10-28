#include "store.hpp"
#include "../Utility/overloaded.hpp"

/*
  Step 1:
    Find everything we need to
*/
namespace expr_module {
  using OwnedExpression = new_expression::OwnedExpression;
  using WeakExpression = new_expression::WeakExpression;
  using Arena = new_expression::Arena;
  using RuleCollector = new_expression::RuleCollector;
  using PrimitiveTypeCollector = new_expression::PrimitiveTypeCollector;
  using OwnedKeySet = new_expression::OwnedKeySet;
  template<class T>
  using WeakKeyMap = new_expression::WeakKeyMap<T>;
  using RuleBody = new_expression::RuleBody;

  namespace {
    void add_dependencies_of_to(WeakExpression target, StoreContext& context, OwnedKeySet& collector) {
      //adds any *declarations* and *axioms* found in target, or its type, or its rules to collector.
      if(collector.contains(target)) return;
      if(context.get_import_index_of(target)) return; //ignore imports
      context.arena.visit(target, mdb::overloaded{
        [&](new_expression::Apply const& apply) {
          add_dependencies_of_to(apply.lhs, context, collector);
          add_dependencies_of_to(apply.rhs, context, collector);
        },
        [&](new_expression::Axiom const& axiom) {
          collector.insert(context.arena.copy(target));
          if(context.type_collector.has_type(target)) {
            add_dependencies_of_to(context.type_collector.get_type_of(target), context, collector);
          }
        },
        [&](new_expression::Declaration const& declaration) {
          collector.insert(context.arena.copy(target));
          if(context.type_collector.has_type(target)) {
            add_dependencies_of_to(context.type_collector.get_type_of(target), context, collector);
          }
          for(auto const& rule : context.rule_collector.declaration_info(target).rules) {
            if(auto* substitution = std::get_if<OwnedExpression>(&rule.replacement)) {
              add_dependencies_of_to(*substitution, context, collector);
            }
            for(auto const& step : rule.pattern_body.steps) {
              if(auto* pattern_match = std::get_if<new_expression::PatternMatch>(&step)) {
                add_dependencies_of_to(pattern_match->substitution, context, collector);
                add_dependencies_of_to(pattern_match->expected_head, context, collector);
              }
            }
          }
        },
        [&](new_expression::Argument const&) {},
        [&](new_expression::Conglomerate const&) {
          std::terminate(); //shouldn't find these!
        },
        [&](new_expression::Data const&) {
          std::terminate(); //no support for these yet!
        }
      });
    }
    struct RegisterState {
      std::uint32_t register_offset; //ignore registers beneath this.
      std::vector<bool> occupied;
      std::uint32_t get_free() {
        for(std::uint32_t i = 0; i < occupied.size(); ++i) {
          if(!occupied[i]) {
            occupied[i] = true;
            return i + register_offset;
          }
        }
        auto ret = occupied.size() + register_offset;
        occupied.push_back(true);
        return ret;
      }
      void free_register(std::uint32_t r) {
        if(r < register_offset) return;
        occupied[r - register_offset] = false;
      }
    };
    std::uint32_t create_expression(WeakExpression target, StoreContext& context, std::vector<step::Any>& steps, RegisterState& state, WeakKeyMap<std::uint32_t>& primitives_to_register) {
      if(auto import_index = context.get_import_index_of(target)) {
        auto ret = state.get_free();
        steps.push_back(step::Embed{
          .import_index = *import_index,
          .output = ret
        });
        return ret;
      }
      return context.arena.visit(target, mdb::overloaded{
        [&](new_expression::Apply const& apply) {
          auto lhs = create_expression(apply.lhs, context, steps, state, primitives_to_register);
          auto rhs = create_expression(apply.rhs, context, steps, state, primitives_to_register);
          state.free_register(lhs);
          state.free_register(rhs);
          auto ret = state.get_free();
          steps.push_back(step::Apply{
            .lhs = lhs,
            .rhs = rhs,
            .output = ret
          });
          return ret;
        },
        [&](new_expression::Axiom const& axiom) {
          return primitives_to_register.at(target);
        },
        [&](new_expression::Declaration const& declaration) {
          return primitives_to_register.at(target);
        },
        [&](new_expression::Argument const& arg) {
          auto ret = state.get_free();
          steps.push_back(step::Argument{
            .index = (std::uint32_t)arg.index,
            .output = ret
          });
          return ret;
        },
        [&](new_expression::Conglomerate const&) -> std::uint32_t {
          std::terminate(); //shouldn't find these!
        },
        [&](new_expression::Data const&) -> std::uint32_t {
          std::terminate(); //no support for these yet!
        }
      });
    }
    void write_rule(std::uint32_t head_register, RuleBody const& body, StoreContext& context, std::vector<step::Any>& steps, RegisterState& state, WeakKeyMap<std::uint32_t>& primitives_to_register) {
      std::vector<std::uint32_t> registers_to_free;
      auto local_register = [&](std::uint32_t input) {
        registers_to_free.push_back(input);
        return input;
      };
      auto write_expr = [&](WeakExpression target) {
        return local_register(create_expression(target, context, steps, state, primitives_to_register));
      };
      std::vector<rule_step::Any> rule_steps;
      for(auto const& step : body.pattern_body.steps) {
        if(std::holds_alternative<new_expression::PullArgument>(step)) {
          rule_steps.push_back(rule_step::PullArgument{});
        } else {
          auto const& match = std::get<new_expression::PatternMatch>(step);
          rule_steps.push_back(rule_step::PatternMatch{
            .substitution = write_expr(match.substitution),
            .expected_head = write_expr(match.expected_head),
            .args_captured = (std::uint32_t)match.args_captured
          });
        }
      }
      steps.push_back(step::Rule{
        .head = head_register,
        .args_captured = (std::uint32_t)body.pattern_body.args_captured,
        .steps = std::move(rule_steps),
        .replacement = rule_replacement::Substitution{
          write_expr(std::get<OwnedExpression>(body.replacement))
        }
      });
      for(auto to_free : registers_to_free) {
        state.free_register(to_free);
      }
    }
  }
  Core store(StoreContext context, StoreInfo info) {
    OwnedKeySet primitives_to_create(context.arena);
    std::vector<step::Any> steps;
    for(auto const& export_expr : info.exports) {
      add_dependencies_of_to(export_expr, context, primitives_to_create);
    }
    /*
      Next: create the requested declarations.
    */
    WeakKeyMap<std::uint32_t> primitives_to_register(context.arena);
    std::uint32_t index = 0;
    for(WeakExpression primitive : primitives_to_create) {
      if(context.arena.holds_axiom(primitive)) {
        steps.push_back(step::Axiom{
          .output = index
        });
      } else {
        steps.push_back(step::Declare{
          .output = index
        });
      }
      primitives_to_register.set(primitive, index++);
    }
    std::uint32_t primitive_size = index;
    RegisterState state{
      .register_offset = primitive_size
    };
    /*
      Next: write the requested type registrations.
    */
    for(auto const& entry : primitives_to_register) {
      if(context.type_collector.has_type(entry.first)) {
        WeakExpression type = context.type_collector.get_type_of(entry.first);
        auto type_index = create_expression(type, context, steps, state, primitives_to_register);
        steps.push_back(step::RegisterType{
          .index = entry.second,
          .type = type_index
        });
        state.free_register(type_index);
      }
    }
    /*
      Then: write the rules.
    */
    for(auto const& entry : primitives_to_register) {
      if(context.arena.holds_declaration(entry.first)) {
        auto const& declaration_info = context.rule_collector.declaration_info(entry.first);
        for(auto const& rule : declaration_info.rules) {
          write_rule(entry.second, rule, context, steps, state, primitives_to_register);
        }
      }
    }
    /*
      Finally: write the exports
    */
    for(auto const& export_expr : info.exports) {
      auto index = create_expression(export_expr, context, steps, state, primitives_to_register);
      steps.push_back(step::Export{
        .value = index
      });
      state.free_register(index);
    }
    destroy_from_arena(context.arena, info.exports);
    return Core {
      .value_import_size = context.get_import_size(),
      .data_type_import_size = 0,
      .c_replacement_import_size = 0,
      .register_count = (std::uint32_t)(state.register_offset + state.occupied.size()),
      .output_size = (std::uint32_t)info.exports.size(),
      .steps = std::move(steps)
    };

  }

}
