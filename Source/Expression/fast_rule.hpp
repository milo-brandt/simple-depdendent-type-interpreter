#ifndef EXPRESSION_FAST_RULE_HPP
#define EXPRESSION_FAST_RULE_HPP

#include <vector>
#include <cstdint>
#include <iostream>
#include <variant>
#include "expression_tree.hpp"

/*
  This file defines utilities to allow for faster pattern matching.
*/
namespace expression::fast_rule {
  /*
    A program is represented by:
      1. Some finite number of registers.
      2. A *program* consisting of a number of statements that may
         request the expansion of any slot, and then list various
         possible continuations by asking...
          ...that the head be a particular external or a data expression of a particular type
          ...with some number of args applied
         and then providing a new place in the program to jump to under these conditions,
         which first specifies where to put the results of the last step.

         A step may also point to a rule to use or declare there to be no matches.
  */
  namespace program_element {
    struct ExpandCase {
      std::uint64_t head_external;
      std::uint32_t arg_count;
      std::uint32_t next_instruction;
    };
    struct CheckDataCase {
      std::uint64_t data_type;
      std::uint32_t next_instruction;
    };
    struct Expand {
      std::uint32_t source_register;
      std::uint32_t base_output;
      std::uint32_t default_next;
      std::vector<ExpandCase> cases;
      std::vector<CheckDataCase> data_checks;
    };
    struct Replace {
      std::uint32_t pattern_index;
      std::vector<std::uint32_t> pattern_arg_registers;
    };
    struct ReplaceData {
      std::uint32_t data_pattern_index;
      std::vector<std::uint32_t> pattern_arg_registers;
    };
    struct Fail {}; //declare that no match exists
    using Instruction = std::variant<Expand, Replace, ReplaceData, Fail>;
    struct Program {
      std::uint32_t registers_needed;
      std::uint32_t args_needed;
      std::vector<Instruction> instructions;
    };
    struct Formatted {
      Program const& program;
    };
    inline Formatted format(Program const& program) { return Formatted{program}; }
    std::ostream& operator<<(std::ostream&, Formatted const&);
  };
  using Program = program_element::Program;
  Program from_patterns(std::vector<indexed_pattern::Pattern> const&, std::vector<indexed_data_pattern::Pattern> const&);
  Program trivial_program();
}

#endif
