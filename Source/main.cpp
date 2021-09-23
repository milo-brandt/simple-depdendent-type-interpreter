#include <fstream>
#include "Expression/interactive_environment.hpp"

/*
void debug_print_expr(expression::tree::Expression const& expr) {
  std::cout << expression::raw_format(expr) << "\n";
}
void debug_print_pattern(expression::pattern::Pattern const& pat) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(pat)) << "\n";
}
void debug_print_rule(expression::Rule const& rule) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
}
*/
int main(int argc, char** argv) {
  std::string last_line = "";

  expression::interactive::Environment environment;
  auto const& u64 = environment.u64();
  auto const& str = environment.str();
  auto vec = expression::data::Vector{environment.axiom_check("Vector", "Type -> Type").head};
  expression::data::builder::RuleMaker rule_maker{environment.context(), u64, str}; //needs to live as long as the rules it creates... meh

  {
    using namespace expression::data::builder;
    namespace tree = expression::tree;
    using Expression = tree::Expression;

    auto add_lambda_rule = [&]<class F>(std::string name, F f) {
      auto manufactured = rule_maker(f);
      environment.name_external(std::move(name), manufactured.head);
      environment.context().add_data_rule(std::move(manufactured.rule));
    };

    auto Bool = environment.axiom_check("Bool", "Type").head;
    auto yes = environment.axiom_check("yes", "Bool").head;
    auto no = environment.axiom_check("no", "Bool").head;
    auto Assert = environment.axiom_check("Assert", "Bool -> Type").head;
    auto witness = environment.axiom_check("witness", "Assert yes").head;

    auto eq = environment.declare_check("eq", "U64 -> U64 -> Bool").head;
    auto lte = environment.declare_check("lte", "U64 -> U64 -> Bool").head;
    auto lt = environment.declare_check("lt", "U64 -> U64 -> Bool").head;
    auto sub_pos = environment.declare_check("sub_pos", "(x : U64) -> (y : U64) -> Assert (lte y x) -> U64").head;

    auto recurse = environment.declare_check("indexed_recurse", "(T : Type) -> (U64 -> T -> T) -> T -> U64 -> T").head;

    add_lambda_rule("sub", [](std::uint64_t x, std::uint64_t y) {
      return x - y;
    });
    add_lambda_rule("add", [](std::uint64_t x, std::uint64_t y) {
      return x + y;
    });
    add_lambda_rule("mul", [](std::uint64_t x, std::uint64_t y) {
      return x * y;
    });
    add_lambda_rule("exp", [](std::uint64_t x, std::uint64_t y) {
      std::uint64_t ret = 1;
      for(std::uint64_t ct = 0; ct < y; ++ct)
        ret *= x;
      return ret;
    });
    add_lambda_rule("len", [](imported_type::StringHolder const& str) {
      return str.size();
    });
    add_lambda_rule("substr", [](imported_type::StringHolder str, std::uint64_t start, std::uint64_t len) {
      return str.substr(start, len);
    });
    add_lambda_rule("cat", [](imported_type::StringHolder lhs, imported_type::StringHolder rhs) {
      return imported_type::StringHolder{std::string{lhs.get_string()} + std::string{rhs.get_string()}};
    });

    auto starts_with = environment.declare_check("starts_with", "String -> String -> Bool").head;
    environment.context().add_data_rule(
      pattern(fixed(starts_with), match(str), match(str)) >> [&, yes, no](imported_type::StringHolder const& prefix, imported_type::StringHolder const& str) {
        return tree::Expression{tree::External{ (str.get_string().starts_with(prefix.get_string())) ? yes : no }};
      }
    );

    environment.context().add_data_rule(
      pattern(fixed(eq), match(u64), match(u64)) >> [&, yes, no](std::uint64_t x, std::uint64_t y) {
        return tree::Expression{tree::External{ (x == y) ? yes : no }};
      }
    );
    environment.context().add_data_rule(
      pattern(fixed(lte), match(u64), match(u64)) >> [&, yes, no](std::uint64_t x, std::uint64_t y) {
        return tree::Expression{tree::External{ (x <= y) ? yes : no }};
      }
    );
    environment.context().add_data_rule(
      pattern(fixed(lt), match(u64), match(u64)) >> [&, yes, no](std::uint64_t x, std::uint64_t y) {
        return tree::Expression{tree::External{ (x < y) ? yes : no }};
      }
    );
    environment.context().add_data_rule(
      pattern(fixed(sub_pos), match(u64), match(u64), fixed(witness)) >> [&](std::uint64_t x, std::uint64_t y) {
        return u64(x - y);
      }
    );

    environment.context().add_data_rule(
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
    auto empty_vec = environment.declare_check("empty_vec", "(T : Type) -> Vector T").head;
    environment.context().add_data_rule(
      pattern(fixed(empty_vec), wildcard) >> [&](tree::Expression type) {
        return vec(std::move(type), {});
      }
    );
    auto push_vec = environment.declare_check("push_vec", "(T : Type) -> Vector T -> T -> Vector T").head;
    environment.context().add_data_rule(
      pattern(fixed(push_vec), wildcard, match(vec), wildcard) >> [&](tree::Expression type, std::vector<tree::Expression> data, tree::Expression then) {
        data.push_back(then);
        return vec(std::move(type), std::move(data));
      }
    );
    auto len_vec = environment.declare_check("len_vec", "(T : Type) -> Vector T -> U64").head;
    environment.context().add_data_rule(
      pattern(fixed(len_vec), ignore, match(vec)) >> [&](std::vector<tree::Expression> const& data) {
        return u64(data.size());
      }
    );
    auto at_vec = environment.declare_check("at_vec", "(T : Type) -> (v : Vector T) -> (n : U64) -> Assert (lt n (len_vec T v)) -> T").head;
    environment.context().add_data_rule(
      pattern(fixed(at_vec), ignore, match(vec), match(u64), fixed(witness)) >> [&](std::vector<tree::Expression> const& data, std::uint64_t index) {
        return data[index];
      }
    );
    auto recurse_vec = environment.declare_check("lfold_vec", "(S : Type) -> (T : Type) -> S -> (S -> T -> S) -> Vector T -> S").head;
    environment.context().add_data_rule(
      pattern(fixed(recurse_vec), ignore, ignore, wildcard, wildcard, match(vec)) >> [&](tree::Expression base, tree::Expression op, std::vector<tree::Expression> const& data) {
        for(auto const& expr : data) {
          base = expression::multi_apply(
            op,
            std::move(base),
            std::move(expr)
          );
        }
        return std::move(base);
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
