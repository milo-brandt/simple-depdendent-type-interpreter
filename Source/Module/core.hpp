#ifndef ARENA_MODULE_CORE_HPP
#define ARENA_MODULE_CORE_HPP

#include <variant>
#include <vector>
#include <cstdint>

namespace expr_module {
  /*
    Modules contain the following basic data:

    1. A number of import slots for...
      (a) values
      (b) Data types
      (c) C++ replacement functions
    2. A number of output slots.
    3. A number of registers, initially empty, which can be manipulated:
      (a) by applying two registers and storing the result in a third
      (b) by declaring a new value of a given type and storing it somewhere
      (c) by declaring a new axiom of a given type and storing it somewhere
      (d) by synthesizing an argument and storing it
      (e) by creating a new rule based off of the existing registers.
  */
  namespace rule_step {
    struct PatternMatch {
      std::uint32_t substitution;
      std::uint32_t expected_head;
      std::uint32_t args_captured;
    };
    struct DataCheck {
      std::uint32_t capture_index;
      std::uint32_t data_type_index; //referenced to an import
    };
    struct PullArgument{};
    using Any = std::variant<PatternMatch, DataCheck, PullArgument>;
  };
  namespace rule_replacement {
    struct Substitution {
      std::uint32_t substitution;
    };
    struct Function {
      std::uint32_t function_index; //referenced to an import. each import should only be used once.
    };
    using Any = std::variant<Substitution, Function>;
  };
  namespace step {
    struct Embed {
      std::uint32_t import_index;
      std::uint32_t output;
    };
    struct Apply {
      std::uint32_t lhs;
      std::uint32_t rhs;
      std::uint32_t output;
    };
    struct Declare {
      std::uint32_t type;
      std::uint64_t output;
    };
    struct Axiom {
      std::uint32_t type;
      std::uint64_t output;
    };
    struct Argument {
      std::uint32_t index;
      std::uint64_t output;
    };
    struct Rule {
      std::uint32_t head;
      std::uint32_t args_captured;
      std::vector<rule_step::Any> steps;
      rule_replacement::Any replacement;
    };
    struct Export {
      std::uint32_t value; //saves the indicated value somewhere
    };
    struct Clear {
      std::uint32_t target; //remove the value from the target register
    };
    using Any = std::variant<Embed, Apply, Declare, Axiom, Argument, Rule, Export, Clear>;
  }
  struct Core {
    std::uint32_t value_import_size;
    std::uint32_t data_type_import_size;
    std::uint32_t c_replacement_import_size;
    std::uint32_t register_count;
    std::uint32_t output_size;
    std::vector<step::Any> steps;
  };
}

#endif
