#include <fstream>
#include "Expression/interactive_environment.hpp"
#include "Expression/expression_debug_format.hpp"

void debug_print_expr(expression::tree::Expression const& expr) {
  std::cout << expression::raw_format(expr) << "\n";
}
void debug_print_pattern(expression::pattern::Pattern const& pat) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(pat)) << "\n";
}
void debug_print_rule(expression::Rule const& rule) {
  std::cout << expression::raw_format(expression::trivial_replacement_for(rule.pattern)) << " -> " << expression::raw_format(rule.replacement) << "\n";
}

expression::interactive::Environment setup_enviroment() {
  expression::interactive::Environment environment;
  auto const& u64 = environment.u64();
  auto const& str = environment.str();
  static bool init_vec = false;
  static auto vec = expression::data::Vector{environment.axiom_check("Vector", "Type -> Type").head};
  //very ugly hack here... need to find somewhere to store vec with lifetime of environment
  if(init_vec) {
    //we didn't run the axiom check because of the static keyword
    //force it manually
    environment.axiom_check("Vector", "Type -> Type");
  } else {
    init_vec = true;
  }
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

    auto iterate = environment.declare_check("iterate", "(T : Type) -> (T -> T) -> T -> U64 -> T").head;

    add_lambda_rule("sub", [](std::uint64_t x, std::uint64_t y) {
      return x - y;
    });
    add_lambda_rule("add", [](std::uint64_t x, std::uint64_t y) {
      return x + y;
    });
    add_lambda_rule("mul", [](std::uint64_t x, std::uint64_t y) {
      return x * y;
    });
    add_lambda_rule("idiv", [](std::uint64_t x, std::uint64_t y) {
      return x / y;
    });
    add_lambda_rule("mod", [](std::uint64_t x, std::uint64_t y) {
      return x % y;
    });
    add_lambda_rule("exp", [](std::uint64_t x, std::uint64_t y) {
      std::uint64_t ret = 1;
      for(std::uint64_t ct = 0; ct < y; ++ct)
        ret *= x;
      return ret;
    });
    add_lambda_rule("len", [](imported_type::StringHolder const& str) {
      return (std::uint64_t)str.size();
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
      pattern(fixed(iterate), ignore, wildcard, wildcard, match(u64)) >> [&](Expression step, Expression base, std::uint64_t count) {
        for(std::uint64_t i = 0; i < count; ++i) {
          base = expression::multi_apply(
            step,
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
    /*
    These lines are very odd! Must be factored out later!
    Programmer be warned!
    */
    environment.context().primitives.empty_vec = empty_vec;
    environment.context().primitives.push_vec = push_vec;

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
  return environment;
}

#ifdef COMPILE_FOR_EMSCRIPTEN

#include <emscripten/bind.h>
#include <sstream>

std::string replace_newlines_with_br(std::string input) {
  while(true) { //this isn't very efficient - but not a bottleneck either
    auto next_new_line = input.find('\n');
    if(next_new_line == std::string::npos) return input;
    input.replace(next_new_line, 1, "<br/>");
  }
}

expression::interactive::Environment& get_last_environment() {
  static expression::interactive::Environment ret = setup_enviroment();
  return ret;
}
void reset_environment() {
  get_last_environment() = setup_enviroment();
}

std::string run_script(std::string script, bool reset) {
  std::stringstream ret;
  std::string_view source = script;
  if(reset) reset_environment();
  get_last_environment().debug_parse(source, ret);
  return replace_newlines_with_br(ret.str());
}
int main(int argc, char** argv) {
  get_last_environment();
}

using namespace emscripten;
EMSCRIPTEN_BINDINGS(my_module) {
    function("run_script", &run_script);
}

#else

int main(int argc, char** argv) {

  auto environment = setup_enviroment();

  if(argc == 2) {
    std::ifstream f(argv[1]);
    if(!f) {
      std::cout << "Failed to read file \"" << argv[1] << "\"\n";
      return -1;
    } else {
      environment = setup_enviroment(); //clean environment
      std::string source_str;
      std::getline(f, source_str, '\0'); //just read the whole file - assuming no null characters in it
      std::string_view source = source_str;
      environment.debug_parse(source);
      return 0;
    }
  } else if(argc > 2) {
    std::cout << "The interpreter expects either a single file to run as an argument or no arguments to run in interactive mode.\n";
    return -1;
  }

  //interactive mode
  std::string last_line = "";
  while(true) {
    std::string line;
    std::getline(std::cin, line);
    if(line.empty()) {
      if(last_line.empty()) continue;
      line = last_line;
    }
    while(line.back() == '\\') { //keep reading
      line.back() = '\n'; //replace backslash with new line
      std::string next_line;
      std::getline(std::cin, next_line);
      line += next_line;
    }
    last_line = line;
    if(line == "q") return 0;
    if(line.starts_with("file ")) {
      std::ifstream f(line.substr(5));
      if(!f) {
        std::cout << "Failed to read file \"" << line.substr(5) << "\"\n";
        continue;
      } else {
        environment = setup_enviroment(); //clean environment
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
#endif
