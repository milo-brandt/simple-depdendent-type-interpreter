#include "Expression/interactive_environment.hpp"
#include "Expression/expression_debug_format.hpp"
#include <fstream>

void debug_print_expr(expression::tree::Expression const& expr) {
  std::cout << expression::raw_format(expr) << "\n";
}
void debug_print_pattern(expression::pattern::Pattern const& pat) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(pat)) << "\n";
}
void debug_print_rule(expression::Rule const& rule) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
}

int main(int argc, char** argv) {
  std::string last_line = "";
  expression::interactive::Environment environment;
  {
    using namespace expression::data::builder;
    namespace tree = expression::tree;
    using Expression = tree::Expression;

    auto const& u64 = environment.u64();
    auto const& str = environment.str();

    auto Bool = environment.axiom_check("Bool", "Type").head;
    auto yes = environment.axiom_check("yes", "Bool").head;
    auto no = environment.axiom_check("no", "Bool").head;

    auto add = environment.declare_check("add", "U64 -> U64 -> U64").head;
    auto mul = environment.declare_check("mul", "U64 -> U64 -> U64").head;
    auto eq = environment.declare_check("eq", "U64 -> U64 -> Bool").head;
    auto recurse = environment.declare_check("indexed_recurse", "(T : Type) -> (U64 -> T -> T) -> T -> U64 -> T").head;

    auto substr = environment.declare_check("substr", "String -> U64 -> String").head;

    environment.context().data_rules.push_back(
      pattern(fixed(add), match(u64), match(u64)) >> [&](std::uint64_t x, std::uint64_t y) {
        return u64(x + y);
      }
    );
    environment.context().data_rules.push_back(
      pattern(fixed(mul), match(u64), match(u64)) >> [&](std::uint64_t x, std::uint64_t y) {
        return u64(x * y);
      }
    );
    environment.context().data_rules.push_back(
      pattern(fixed(eq), match(u64), match(u64)) >> [&, yes, no](std::uint64_t x, std::uint64_t y) {
        return tree::Expression{tree::External{ (x == y) ? yes : no }};
      }
    );
    environment.context().data_rules.push_back(
      pattern(fixed(recurse), ignore, wildcard, wildcard, match(u64)) >> [&](Expression step, Expression base, std::uint64_t count) {
        for(std::uint64_t i = 0; i < count; ++i) {
          base = expression::multi_apply(
            step,
            u64(i),
            std::move(base)
          );
        }
        return std::move(base);
      }
    );
    environment.context().data_rules.push_back(
      pattern(fixed(substr), match(str), match(u64)) >> [&](auto str_holder, std::uint64_t amt) {
        return str({std::make_shared<std::string>(str_holder.data->substr(amt))});
      }
    );
  }


  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line.empty()) {
      if(last_line.empty()) continue;
      line = last_line;
    }
    last_line = line;
    if(line == "q") return 0;
    if(line.starts_with("file ")) {
      std::ifstream f(line.substr(5));
      if(!f) {
        std::cout << "Failed to read file \"" << line.substr(5) << "\"\n";
        continue;
      } else {
        std::string total;
        std::getline(f, total, '\0'); //just read the whole file - assuming no null characters in it :P
        std::cout << "Contents of file:\n" << total << "\n";
        line = std::move(total);
      }
    }
    std::string_view source = line;
    environment.debug_parse(source);
  }
  return 0;
}

/*
block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; declare add : Nat -> Nat -> Nat; rule add zero x = x; rule add (succ x) y = succ (add x y); declare mul : Nat -> Nat -> Nat; rule mul zero x = zero; rule mul (succ x) y = add y (mul x y); mul (succ \\ succ \\ succ \\ succ zero) (succ \\ succ \\ succ \\ succ \\ succ zero) }
*/

/*
block { axiom String : Type; axiom One : Type; axiom Parser : Type -> Type; axiom Recognize : (T: Type) -> String -> Parser T -> Parser T; axiom Accept : (T : Type) -> T -> Parser T; axiom dot : String; f Recognize _ dot \\ Recognize _ dot \\ Accept _ Type }
*/
/*
block { axiom Id : Type -> Type; axiom pure : (T : Type) -> T -> Id T; declare extract : (T : Type) -> Id T -> T; rule extract T (pure T x) = x; extract }
block { axiom Id : Type -> Type; axiom pure : (T : Type) -> T -> Id T; pure }

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Vec : Nat -> Type; axiom empty : Vec zero; axiom cons : (n : Nat) -> Nat -> Vec n -> Vec (succ n); declare fold : Nat -> (Nat -> Nat -> Nat) -> (n : Nat) -> Vec n -> Nat; fold }

block { axiom Nat : Type; declare mul : Nat -> Nat -> Nat; axiom Square : Nat -> Type; axiom witness : (n : Nat) -> Square (mul n n); declare sqrt : (n : Nat) -> (Square n) -> Nat; rule sqrt (mul n n) (witness n) = n; sqrt }

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Vec : Nat -> Type; axiom empty : Vec zero; axiom cons : (n : Nat) -> Nat -> Vec n -> Vec (succ n); declare fold : Nat -> (Nat -> Nat -> Nat) -> (n : Nat) -> Vec n -> Nat; rule fold base combine zero empty = base; rule fold base combine (succ n) (cons n head tail) = fold (combine base head) combine n tail; declare mul : Nat -> Nat -> Nat; declare fst : Nat; fold fst mul _ (cons _ zero (cons _ (succ zero) empty)) }
block {
  axiom Nat : Type;
  axiom zero : Nat;
  axiom succ : Nat -> Nat;
  axiom Routine : Type -> Type;
  axiom return : (T : Type) -> T -> Routine T;
  axiom read : (T : Type) -> (Nat -> Routine T) -> Routine T;

  declare bind : (S : Type) -> (T : Type) -> Routine S -> (S -> Routine T) -> Routine T;
  bind S T (return S x) then = then x;
  bind S T (read S f) then = read T \n.bind S T (f n) then;
}

block { axiom Nat : Type; axiom zero : Nat; axiom succ : Nat -> Nat; axiom Routine : Type -> Type; axiom return : (T : Type) -> T -> Routine T; axiom read : (T : Type) -> (Nat -> Routine T) -> Routine T; declare bind : (S : Type) -> (T : Type) -> Routine S -> (S -> Routine T) -> Routine T; rule bind S T (return S x) then = then x; rule bind S T (read S f) then = read T \n.bind S T (f n) then; bind }


block { axiom Id : (T : Type) -> T -> T -> Type; axiom refl : (T : Type) -> (x : T) -> Id T x x; declare compose : (T : Type) -> (x : T) -> (y : T) -> (z : T) -> Id T x y -> Id T y z -> Id T x z; rule compose T x x x (refl T x) (refl T x) = refl T x; compose }

...this could be a good use case for coroutines, probably? - let things wait for whatever's blocking their progress
*/
