#include "resolved_tree.hpp"

namespace compiler::flat {
  struct PatternTree{};
  namespace step {
    struct Declare {
      std::uint64_t context;
      std::uint64_t type;
    };
    struct ContextExtend {
      std::uint64_t context;
      std::uint64_t type;
    };
    struct Rule {
      std::uint64_t head;
      std::vector<PatternTree> args_to_match;
      std::uint64_t output; //in appropriate context
    };
    struct InductiveDeclaration {
      std::vector<std::uint64_t> type_parameters;
      struct Constructor {
        std::vector<std::uint64_t> parameters;
        std::vector<std::uint64_t> output_type_parameters;
      };
      std::vector<Constructor> constructors;
    };
  }
  /*

  Maybe have a section for declarations to deal with mutual induction more easily?

  Declare add : Nat -> Nat -> Nat
  Declare mul : Nat -> Nat -> Nat
  Rule mul zero y = zero
  Rule mul (succ x) y = add (mul x y) y
  Rule add zero y = y
  Rule add (succ x) y = succ (add x y)

  Declare Even : Nat -> Type
  Declare Odd : Nat -> Type
  Inductive family over Nat as Even {
    zero : Even 0
    after_odd : (x : Nat) -> Odd x -> Even (succ x)
  }
  Inductive family over Nat as Odd {
    after_even : (x : Nat) -> Even x -> Odd (succ x)
  }





  */
  /*
    Stacks for...
      1. Values
      2. Contexts
  */
  /*
  \f.\x.f (f x)
  1. Lambda [f]
    2. Lambda [x]
      3. Apply
        4. Arg(0)
        5. Apply
          6. Arg(0)
          7. Arg(1)

    Variable arg_type_1 : Type in empty_context
    Context ctx_1 = empty_context, arg_type_1
    Variable arg_type_2 : Type in ctx_1
    Context ctx_2 = ctx_1, arg_type_2

    Variable domain_5 : Type in ctx_2
    Variable codomain_5 : Type in ctx_2, domain_5
    Variable function_5 : domain_5 -> codomain_5 in ctx_2
    Variable argument_5 : domain_5 in ctx_2

    Cast from arg_type_1 to (domain_5 -> codomain_5) storing arg(0) as function_5.
    Cast from arg_type_2 to domain_5 storing arg(1) as argument_5.

    Variable domain_3 : Type in ctx_2
    Variable codomain_3 : Type in ctx_2, domain_3

    Cast from arg_type_1 to (domain_3 -> codomain_3) storing arg(0) as function_3
    Cast from arg_type_2 to domain_3 storing (function_5 argument_5) as argument_3

    Lambda inner with body (function_3 argument_3)
    Lambda outer with body inner

    Return outer

    (Then feed this to solver, I guess)
  */
  namespace instruction {
    //All members are variables refering to the position of a prior instruction.
    //Context indices of -1 refer to an empty context
    struct DeclareVariable {
      std::uint64_t context;
      std::uint64_t type;
    };
    struct DeclareContext {
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
    struct Lambda {
      std::uint64_t body;
    };
    struct Cast {
      std::uint64_t from_value;
      std::uint64_t to_type;
    };
    struct Argument { //top argument of context
      std::uint64_t context;
      std::uint64_t index;
    };
    using Instruction = std::variant<DeclareVariable, DeclareContext, Embed, Apply, Lambda, Cast, Argument>;
    using Program = std::vector<Instruction>;

    inline bool operator==(DeclareVariable const& lhs, DeclareVariable const& rhs) { return lhs.context == rhs.context && lhs.type == rhs.type; }
    inline bool operator==(DeclareContext const& lhs, DeclareContext const& rhs) { return lhs.base_context == rhs.base_context && lhs.next_type == rhs.next_type; }
    inline bool operator==(Embed const& lhs, Embed const& rhs) { return lhs.context == rhs.context && lhs.embed_index == rhs.embed_index; }
    inline bool operator==(Apply const& lhs, Apply const& rhs) { return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs; }
    inline bool operator==(Lambda const& lhs, Lambda const& rhs) { return lhs.body && lhs.body; }
    inline bool operator==(Cast const& lhs, Cast const& rhs) { return lhs.from_value == rhs.from_value && lhs.to_type == rhs.to_type; }
    inline bool operator==(Argument const& lhs, Argument const& rhs) { return lhs.context == rhs.context && lhs.index == rhs.index; }

    inline std::ostream& operator<<(std::ostream& o, DeclareVariable const& var) { return o << "Declare variable of type " << var.type << " in context " << var.context; }
    inline std::ostream& operator<<(std::ostream& o, DeclareContext const& var) { return o << "Declare context above " << var.base_context << " with type " << var.next_type; }
    inline std::ostream& operator<<(std::ostream& o, Embed const& var) { return o << "Embed " << var.embed_index << " in context " << var.context; }
    inline std::ostream& operator<<(std::ostream& o, Apply const& var) { return o << "Apply " << var.lhs << " to " << var.rhs; }
    inline std::ostream& operator<<(std::ostream& o, Lambda const& var) { return o << "Lambda with body " << var.body; }
    inline std::ostream& operator<<(std::ostream& o, Cast const& var) { return o << "Cast " << var.from_value << " to type " << var.to_type; }
    inline std::ostream& operator<<(std::ostream& o, Argument const& var) { return o << "Argument index " << var.index << " from context " << var.context; }
    inline std::ostream& operator<<(std::ostream& o, Instruction const& var) { return std::visit([&](auto const& var) -> std::ostream& { return o << var; }, var); }
    inline std::ostream& operator<<(std::ostream& o, Program const& program) {
      std::size_t count;
      for(auto const& instruction : program) {
        if(count) o << "\n";
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
