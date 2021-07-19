#ifndef FLAT_INSTRUCTION_HPP
#define FLAT_INSTRUCTION_HPP

#include "resolved_tree.hpp"

namespace compiler::flat {
  namespace instruction {
    //All members are variables refering to the position of a prior instruction.
    //Context indices of -1 refer to an empty context
    enum class Foundation {
      type, arrow, empty_context
    };
    struct Hole {
      std::uint64_t context;
      std::uint64_t type;
    };
    struct Declaration {
      std::uint64_t context;
      std::uint64_t type;
    };
    struct Context {
      std::uint64_t base_context;
      std::uint64_t next_type;
    };
    struct Embed {
      std::uint64_t context;
      std::uint64_t embed_index; //external index
    };
    struct Apply {
      std::uint64_t lhs;
      std::uint64_t rhs;
    };
    struct Rule {
      std::uint64_t pattern;
      std::uint64_t replacement;
    };
    struct Cast {
      std::uint64_t from_value;
      std::uint64_t to_type;
    };
    using Instruction = std::variant<Foundation, Hole, Declaration, Context, Embed, Apply, Rule, Cast>;
    struct Program {
      std::vector<Instruction> instructions;
      std::uint64_t ret_index;
    };
    inline bool operator==(Hole const& lhs, Hole const& rhs) { return lhs.context == rhs.context && lhs.type == rhs.type; }
    inline bool operator==(Declaration const& lhs, Declaration const& rhs) { return lhs.context == rhs.context && lhs.type == rhs.type; }
    inline bool operator==(Context const& lhs, Context const& rhs) { return lhs.base_context == rhs.base_context && lhs.next_type == rhs.next_type; }
    inline bool operator==(Embed const& lhs, Embed const& rhs) { return lhs.context == rhs.context && lhs.embed_index == rhs.embed_index; }
    inline bool operator==(Apply const& lhs, Apply const& rhs) { return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs; }
    inline bool operator==(Rule const& lhs, Rule const& rhs) { return lhs.pattern == rhs.pattern && lhs.replacement == rhs.replacement; }
    inline bool operator==(Cast const& lhs, Cast const& rhs) { return lhs.from_value == rhs.from_value && lhs.to_type == rhs.to_type; }

    inline std::ostream& operator<<(std::ostream& o, Foundation const& var) { constexpr const char* str[] = {"Type","Arrow","empty_context"}; return o << str[(std::size_t)var]; }
    inline std::ostream& operator<<(std::ostream& o, Hole const& var) { return o << "Hole of type " << var.type << " in context " << var.context; }
    inline std::ostream& operator<<(std::ostream& o, Declaration const& var) { return o << "Declaration of type " << var.type << " in context " << var.context; }
    inline std::ostream& operator<<(std::ostream& o, Context const& var) { return o << "Context above " << var.base_context << " with type " << var.next_type; }
    inline std::ostream& operator<<(std::ostream& o, Embed const& var) { return o << "Embed " << var.embed_index << " in context " << var.context; }
    inline std::ostream& operator<<(std::ostream& o, Apply const& var) { return o << "Apply " << var.lhs << " to " << var.rhs; }
    inline std::ostream& operator<<(std::ostream& o, Rule const& var) { return o << "Rule " << var.pattern << " -> " << var.replacement; }
    inline std::ostream& operator<<(std::ostream& o, Cast const& var) { return o << "Cast " << var.from_value << " to type " << var.to_type; }
    inline std::ostream& operator<<(std::ostream& o, Instruction const& var) { return std::visit([&](auto const& var) -> std::ostream& { return o << var; }, var); }
    inline std::ostream& operator<<(std::ostream& o, Program const& program) {
      std::size_t count;
      for(auto const& instruction : program.instructions) {
        if(count) o << "\n";
        if(program.ret_index == count) {
          o << "***";
        }
        o << (count++) << ": " << instruction;
      }
      return o;
    }
  }
  namespace explanation {
    enum class Reason {
      literal_hole, apply_domain, apply_codomain, apply_function_cast, apply_argument_cast, argument, embed, lambda, lambda_type
    };
    struct LocatedReason {
      Reason reason;
      resolution::path::Path path;
    };
    using Explanation = std::vector<LocatedReason>;
  }
  std::pair<instruction::Program, explanation::Explanation> flatten_tree(compiler::resolution::output::Tree const& tree);
}

#endif
