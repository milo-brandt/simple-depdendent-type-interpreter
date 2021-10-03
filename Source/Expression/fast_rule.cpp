#include "fast_rule.hpp"
#include "../Utility/grid.hpp"
#include <variant>
#include <optional>
#include <unordered_set>

namespace expression::fast_rule {
  namespace {
    struct PatternUnfolding {
      indexed_pattern::Pattern const* head;
      std::vector<indexed_pattern::Pattern const*> args;
    };
    PatternUnfolding unfold_ref(indexed_pattern::Pattern const* pat) {
      PatternUnfolding ret{
        .head = pat
      };
      while(auto const& apply = ret.head->get_if_apply()) {
        ret.head = &apply->lhs;
        ret.args.push_back(&apply->rhs);
      }
      std::reverse(ret.args.begin(), ret.args.end());
      return ret;
    }
    struct DataPatternUnfolding {
      indexed_data_pattern::Pattern const* head;
      std::vector<indexed_data_pattern::Pattern const*> args;
    };
    DataPatternUnfolding unfold_ref(indexed_data_pattern::Pattern const* pat) {
      DataPatternUnfolding ret{
        .head = pat
      };
      while(auto const& apply = ret.head->get_if_apply()) {
        ret.head = &apply->lhs;
        ret.args.push_back(&apply->rhs);
      }
      std::reverse(ret.args.begin(), ret.args.end());
      return ret;
    }
    struct TableauxCase {
      std::uint64_t external_head;
      std::uint32_t arg_count;
      friend bool operator==(TableauxCase const& lhs, TableauxCase const& rhs) {
        return lhs.external_head == rhs.external_head && lhs.arg_count == rhs.arg_count;
      }
    };
    struct TableauxDataCheck {
      std::uint64_t data_type;
      friend bool operator==(TableauxDataCheck const& lhs, TableauxDataCheck const& rhs) {
        return lhs.data_type == rhs.data_type;
      }
    };
    struct TableauxRequest {
      std::size_t register_index;
      std::vector<TableauxCase> cases;
      std::vector<TableauxDataCheck> data_checks;
    };
    template<bool data_version>
    struct HalfTableaux {
      using Pattern = std::conditional_t<data_version, indexed_data_pattern::Pattern, indexed_pattern::Pattern>;
      using Apply = std::conditional_t<data_version, indexed_data_pattern::Apply, indexed_pattern::Apply>;
      using Wildcard = std::conditional_t<data_version, indexed_data_pattern::Wildcard, indexed_pattern::Wildcard>;
      using Request = std::conditional_t<data_version, TableauxDataCheck, TableauxCase>;
      using Data = data_pattern::Data;
      struct Match {
        std::size_t pattern_index;
        std::vector<std::uint32_t> pattern_arg_registers;
      };
      using Entry = std::variant<std::monostate, Pattern const*, std::uint64_t>;
      //either: nothing, a constraint, or a match index
      static Entry entry_from_constraint(Pattern const* pattern) {
        if(auto* wildcard = pattern->get_if_wildcard()) {
          return Entry{(std::uint64_t)wildcard->match_index};
        } else {
          return Entry{pattern};
        }
      }
      static Entry entry_from_match(std::uint64_t index) {
        return Entry{index};
      }
      std::vector<std::size_t> patterns_represented;
      mdb::Grid<Entry> matches_left;
      static HalfTableaux from_pattern_list(std::vector<Pattern> const& patterns, std::size_t arg_count) {
        mdb::Grid<Entry> matches_left{arg_count, patterns.size(), Entry{}}; //make an empty grid.
        std::vector<std::size_t> patterns_represented;
        for(std::size_t index = 0; index < patterns.size(); ++index) {
          patterns_represented.push_back(index);
        }
        for(std::size_t pattern_index = 0; pattern_index < patterns.size(); ++pattern_index) {
          auto unfolding = unfold_ref(&patterns[pattern_index]);
          //should possibly assert that each root now points to the head being defined.
          if(unfolding.args.size() > arg_count) std::terminate();
          for(std::size_t arg_index = 0; arg_index < unfolding.args.size(); ++arg_index) {
            matches_left(arg_index, pattern_index) = entry_from_constraint(unfolding.args[arg_index]);
          }
          //any terms beyond the pattern's depth is Entry{} - no constraint
        }
        return {
          .patterns_represented = std::move(patterns_represented),
          .matches_left = std::move(matches_left)
        };
      }
      bool empty() const {
        return patterns_represented.empty();
      }
      std::optional<Match> find_matched_pattern() const {
        for(std::size_t pattern_row = 0; pattern_row < matches_left.height(); ++pattern_row) {
          for(std::size_t register_index = 0; register_index < matches_left.width(); ++register_index) {
            // first check if this row is okay
            auto const& entry = matches_left(register_index, pattern_row);
            if(entry.index() == 1) {
              goto GO_TO_NEXT_ROW;
            }
          }
          //At this point, we have verified a match.
          {
            std::vector<std::uint32_t> pattern_arg_registers;
            for(std::size_t register_index = 0; register_index < matches_left.width(); ++register_index) {
              auto const& entry = matches_left(register_index, pattern_row);
              if(auto* arg_index = std::get_if<2>(&entry)) {
                pattern_arg_registers.resize(*arg_index + 1, (std::uint32_t)-1);
                pattern_arg_registers[*arg_index] = register_index;
              }
            }
            return Match{
              .pattern_index = patterns_represented[pattern_row],
              .pattern_arg_registers = std::move(pattern_arg_registers)
            };
          }
        GO_TO_NEXT_ROW:;
        }
        return std::nullopt;
      }
      std::vector<Request> request_register(std::size_t register_index) const {
        //return requests, whether a default case is desired
        struct RequestHash {
          std::hash<std::uint64_t> h1;
          std::hash<std::uint32_t> h2;
          auto operator()(Request const& c) const {
            if constexpr(data_version) {
              return h1(c.data_type);
            } else {
              return h1(c.external_head) + 1735686341*h2(c.arg_count);
            }
          }
        };
        std::unordered_set<Request, RequestHash> cases_needed;
        for(std::size_t pattern_row = 0; pattern_row < matches_left.height(); ++pattern_row) {
          auto const& entry = matches_left(register_index, pattern_row);
          if(auto* constraint = std::get_if<1>(&entry)) {
            auto unfolding = unfold_ref(*constraint);
            if constexpr(data_version) {
              if(auto* data = unfolding.head->get_if_data()) {
                if(unfolding.args.size() > 0) std::terminate();
                cases_needed.insert(TableauxDataCheck{
                  data->type_index
                });
              }
            } else {
              cases_needed.insert(TableauxCase{
                unfolding.head->get_fixed().external_index,
                (std::uint32_t)unfolding.args.size()
              });
            }
          }
        }
        std::vector<Request> cases_vec;
        for(auto const& r : cases_needed) cases_vec.push_back(r);
        return cases_vec;
      }
      std::size_t get_next_request_index() const { //Call empty() and find_matched_pattern() first. Do not call this unless neither succeeds
        for(std::size_t register_index = 0; register_index < matches_left.width(); ++register_index) {
          for(std::size_t pattern_row = 0; pattern_row < matches_left.height(); ++pattern_row) {
            if(matches_left(register_index, pattern_row).index() == 1) {
              return register_index;
            }
          }
        }
        return (std::size_t)-1;
      }
      HalfTableaux apply_external_match(std::size_t register_index, std::size_t base_index, std::uint64_t external_result, std::size_t arg_count) const {
        //reject any patterns requiring a different value; expand any requiring this value; ignore others.
        std::size_t register_count = std::max(matches_left.width(), base_index + arg_count);
        auto augmented_grid = mdb::grid_from_function(register_count, matches_left.height(), [&](std::size_t register_index, std::size_t pattern_row) {
          if(register_index < matches_left.width()) return matches_left(register_index, pattern_row);
          else return Entry{}; //empty entry
        }); //Copy input grid with new columns for any new registers, putting Entry{} in new places.
        std::vector<std::size_t> successful_rows;
        for(std::size_t pattern_row = 0; pattern_row < matches_left.height(); ++pattern_row) {
          auto const& entry = matches_left(register_index, pattern_row);
          if(entry.index() != 1) { //not constrained; do nothing.
            successful_rows.push_back(pattern_row);
          } else {
            auto const* constraint = std::get<1>(entry);
            auto unfolding = unfold_ref(constraint);
            if constexpr(data_version) {
              if(unfolding.head->holds_data()) {
                continue; //this row doesn't match.
              }
            }
            if(unfolding.head->get_fixed().external_index != external_result || unfolding.args.size() != arg_count) {
              continue; //this row doesn't match.
            }
            successful_rows.push_back(pattern_row);
            augmented_grid(register_index, pattern_row) = Entry{}; //we've satisfied the original constraint
            for(std::size_t arg_index = 0; arg_index < arg_count; ++arg_index) {
              augmented_grid(base_index + arg_index, pattern_row) = entry_from_constraint(unfolding.args[arg_index]);
            }
          }
        }
        std::vector<std::size_t> patterns_represented;
        for(auto row : successful_rows) {
          patterns_represented.push_back(this->patterns_represented[row]);
        }
        return HalfTableaux{
          .patterns_represented = std::move(patterns_represented),
          .matches_left = mdb::grid_from_function(register_count, successful_rows.size(), [&](std::size_t register_index, std::size_t row) {
            return augmented_grid(register_index, successful_rows[row]);
          })
        };
      }
      HalfTableaux apply_data_match(std::size_t register_index, std::uint64_t type_index) const  {
        std::vector<std::size_t> successful_rows;
        auto derived_matches = matches_left;
        for(std::size_t pattern_row = 0; pattern_row < matches_left.height(); ++pattern_row) {
          auto const& entry = matches_left(register_index, pattern_row);
          if(entry.index() != 1) { //not constrained; do nothing.
            successful_rows.push_back(pattern_row);
          } else {
            if constexpr(data_version) {
              auto const* constraint = std::get<1>(entry);
              if(auto const* data = constraint->get_if_data()) {
                if(data->type_index == type_index) {
                  derived_matches(register_index, pattern_row) = entry_from_match(data->match_index);
                  successful_rows.push_back(pattern_row);
                }
              }
            }
          }
        }
        std::vector<std::size_t> patterns_represented;
        for(auto row : successful_rows) {
          patterns_represented.push_back(this->patterns_represented[row]);
        }
        return HalfTableaux{
          .patterns_represented = std::move(patterns_represented),
          .matches_left = mdb::grid_from_function(matches_left.width(), successful_rows.size(), [&](std::size_t register_index, std::size_t row) {
            return derived_matches(register_index, successful_rows[row]);
          })
        };
      }
      HalfTableaux apply_default_match(std::size_t register_index) const {
        std::vector<std::size_t> successful_rows;
        for(std::size_t pattern_row = 0; pattern_row < matches_left.height(); ++pattern_row) {
          auto const& entry = matches_left(register_index, pattern_row);
          if(entry.index() != 1) { //not constrained; do nothing.
            successful_rows.push_back(pattern_row);
          }
        }
        std::vector<std::size_t> patterns_represented;
        for(auto row : successful_rows) {
          patterns_represented.push_back(this->patterns_represented[row]);
        }
        return HalfTableaux{
          .patterns_represented = std::move(patterns_represented),
          .matches_left = mdb::grid_from_function(matches_left.width(), successful_rows.size(), [&](std::size_t register_index, std::size_t row) {
            return matches_left(register_index, successful_rows[row]);
          })
        };
      }
    };
    struct Tableaux {
      HalfTableaux<false> patterns;
      HalfTableaux<true> data_patterns;
      static Tableaux from_pattern_list(std::vector<indexed_pattern::Pattern> const& patterns, std::vector<indexed_data_pattern::Pattern> const& data_patterns, std::size_t arg_count) {
        return {
          .patterns = HalfTableaux<false>::from_pattern_list(patterns, arg_count),
          .data_patterns = HalfTableaux<true>::from_pattern_list(data_patterns, arg_count)
        };
      }
      bool empty() const {
        return patterns.empty() && data_patterns.empty();
      }
      Tableaux apply_external_match(std::size_t register_index, std::size_t base_index, std::uint64_t external_result, std::size_t arg_count) const  {
        return {
          .patterns = patterns.apply_external_match(register_index, base_index, external_result, arg_count),
          .data_patterns = data_patterns.apply_external_match(register_index, base_index, external_result, arg_count)
        };
      }
      Tableaux apply_data_match(std::size_t register_index, std::uint64_t type_index) const {
        return {
          .patterns = patterns.apply_data_match(register_index, type_index),
          .data_patterns = data_patterns.apply_data_match(register_index, type_index)
        };
      }
      Tableaux apply_default_match(std::size_t register_index) const {
        return {
          .patterns = patterns.apply_default_match(register_index),
          .data_patterns = data_patterns.apply_default_match(register_index)
        };
      }
      std::optional<HalfTableaux<false>::Match> find_matched_pattern() const {
        return patterns.find_matched_pattern();
      }
      std::optional<HalfTableaux<true>::Match> find_matched_data_pattern() const {
        return data_patterns.find_matched_pattern();
      }
      TableauxRequest get_next_request() const {
        auto index = [&] {
          auto pattern_index = patterns.get_next_request_index();
          if(pattern_index == (std::size_t)-1) {
            return data_patterns.get_next_request_index();
          } else {
            return pattern_index;
          }
        }();
        auto external_matches = patterns.request_register(index);
        auto data_checks = data_patterns.request_register(index);
        return {
          .register_index = index,
          .cases = std::move(external_matches),
          .data_checks = std::move(data_checks)
        };
      }
      std::size_t register_count() const {
        return patterns.matches_left.width();
      }
    };
    struct ProgramBuilder {
      std::uint32_t registers_needed = 0;
      std::vector<program_element::Instruction> instructions;
      std::size_t write_tableaux(Tableaux const& tableaux) { //returns index in instructions.
        if(tableaux.register_count() > registers_needed) { //expand total register count if needed
          registers_needed = tableaux.register_count();
        }
        auto ret = instructions.size();
        instructions.push_back(program_element::Fail{}); //add placeholder
        auto place_instruction = [&]<class V>(V&& value) {
          instructions[ret] = std::forward<V>(value);
          return ret;
        };
        if(tableaux.empty()) {
          return ret; //nothing to do. keep the fail as the instruction
        } else if(auto ret = tableaux.find_matched_pattern()) {
          return place_instruction(program_element::Replace{
            .pattern_index = (std::uint32_t)ret->pattern_index,
            .pattern_arg_registers = std::move(ret->pattern_arg_registers)
          });
        } else if(auto ret = tableaux.find_matched_data_pattern()) {
          return place_instruction(program_element::ReplaceData{
            .data_pattern_index = (std::uint32_t)ret->pattern_index,
            .pattern_arg_registers = std::move(ret->pattern_arg_registers)
          });
        } else {
          auto next_request = tableaux.get_next_request();
          auto base_index = tableaux.register_count();
          std::vector<program_element::ExpandCase> expand_cases;
          for(auto const& c : next_request.cases) {
            expand_cases.push_back({
              .head_external = c.external_head,
              .arg_count = c.arg_count,
              .next_instruction = (std::uint32_t)write_tableaux(tableaux.apply_external_match(
                next_request.register_index,
                base_index,
                c.external_head,
                c.arg_count
              ))
            });
          }
          std::vector<program_element::CheckDataCase> data_checks;
          for(auto const& c : next_request.data_checks) {
            data_checks.push_back({
              .data_type = c.data_type,
              .next_instruction = (std::uint32_t)write_tableaux(tableaux.apply_data_match(
                next_request.register_index,
                c.data_type
              ))
            });
          }
          auto default_case = (std::uint32_t)write_tableaux(tableaux.apply_default_match(next_request.register_index));
          return place_instruction(program_element::Expand{
            .source_register = (std::uint32_t)next_request.register_index,
            .base_output = (std::uint32_t)base_index,
            .default_next = default_case,
            .cases = std::move(expand_cases),
            .data_checks = std::move(data_checks)
          });
        }
      }
      program_element::Program take(std::uint32_t args_needed) && {
        return {
          .registers_needed = registers_needed,
          .args_needed = args_needed,
          .instructions = std::move(instructions)
        };
      }
    };

    template class HalfTableaux<false>;
    template class HalfTableaux<true>;
  };

  //e.g. if we had f (succ (succ x)) = ... and f (succ zero) = ... and f zero = ..., we would start
  program_element::Program from_patterns(std::vector<indexed_pattern::Pattern> const& patterns, std::vector<indexed_data_pattern::Pattern> const& data_patterns) {
    std::uint32_t args_needed = 0;
    for(auto const& pattern : patterns) {
      auto arg_count = unfold_ref(&pattern).args.size();
      if(arg_count > args_needed) args_needed = (std::uint32_t)arg_count;
    }
    for(auto const& pattern : data_patterns) {
      auto arg_count = unfold_ref(&pattern).args.size();
      if(arg_count > args_needed) args_needed = (std::uint32_t)arg_count;
    }
    ProgramBuilder builder;
    builder.write_tableaux(Tableaux::from_pattern_list(patterns, data_patterns, args_needed));
    return std::move(builder).take(args_needed);
  }
  Program trivial_program() {
    return {
      .registers_needed = 0,
      .args_needed = 0,
      .instructions = {program_element::Fail{}}
    };
  }
  namespace program_element {
    std::ostream& operator<<(std::ostream& o, Formatted const& f) {
      std::size_t index = 0;
      o << "A program needing " << f.program.args_needed << " args and " << f.program.registers_needed << " registers.\n";
      for(auto const& instruction : f.program.instructions) {
        o << "\n" << (index++) << ": ";
        std::visit(mdb::overloaded{
          [&](Fail const&) {
            o << "FAIL";
          },
          [&](Replace const& replace) {
            o << "REPLACE INDEX " << replace.pattern_index << "\nARGS:";
            for(auto const& r : replace.pattern_arg_registers) {
              o << " " << r;
            }
          },
          [&](ReplaceData const& replace) {
            o << "REPLACE DATA INDEX " << replace.data_pattern_index << "\nARGS:";
            for(auto const& r : replace.pattern_arg_registers) {
              o << " " << r;
            }
          },
          [&](Expand const& expand) {
            o << "EXPAND REGISTER " << expand.source_register << "\nBASE OUTPUT " << expand.base_output << "\nDEFAULT NEXT: " << expand.default_next;
            for(auto const& c : expand.cases) {
              o << "\nCASE: HEAD " << c.head_external << " WITH " << c.arg_count << " ARGS -> " << c.next_instruction;
            }
            for(auto const& c : expand.data_checks) {
              o << "\nCHECK: TYPE " << c.data_type << " -> " << c.next_instruction;
            }
          }
        }, instruction);
      }
      return o;
    }
  }
}


/*
  Possible binary format in case further optimization to compact memory might help:

  An INSTRUCTION is a term consisting of:
    std::uint32_t discriminator; (0 = instruction_expand, 1 = instruction_substitute, 2 = instruction_substitute_data)
    BODY

  the BODY of instruction_expand consists of:
    std::uint32_t source_register; (Which register to expand)
    std::uint32_t base_output; (The start of a contiguous block in which to place outputs)
    std::uint32_t external_case_count; (The number of cases)
    std::uint32_t data_case_count; (The number of data cases)
    CASE * external_case_count (externals)
    CASE * data_case_count (data)

  where a CASE consists of:
    std::uint32_t head_data; (either external index or data type index, depending on whether it's a normal case or data case)
    std::uint32_t arg_count; (number of args applied to it)
    std::uint32_t next_instruction; (where to jump to in program after this)

  the BODY of instruction_substitute or instruction_substitute_data consists of:
    std::uint32_t pattern_index; (Matches up to the program's list of patterns)
    std::uint32_t pattern_arg_count; (Number of args the pattern requires)
    std::uint32_t[pattern_arg_count] arg_registers; (A list of which registers contain the values for arg0, ..., argN of the pattern)

*/
