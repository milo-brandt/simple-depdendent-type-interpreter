#ifndef EXPRESSION_INTERACTIVE_ENVIRONMENT_HPP
#define EXPRESSION_INTERACTIVE_ENVIRONMENT_HPP

#include "evaluation_context.hpp"
#include "data_helper.hpp"
#include "../ImportedTypes/string_holder.hpp"

namespace expression::interactive {
  class Environment;
  class ParseResult {
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend Environment;
    ParseResult(std::unique_ptr<Impl> impl);
  public:
    ParseResult(ParseResult&&);
    ParseResult& operator=(ParseResult&&);
    ~ParseResult();
    bool has_result() const;
    bool is_fully_solved() const;
    TypedValue const& get_result() const; //only call after checking it has a result!
    TypedValue get_reduced_result() const;
    void print_errors_to(std::ostream&) const;
    void print_value(std::ostream&, tree::Expression val) const;
  };
  class Environment {
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend ParseResult;
  public:
    Environment();
    Environment(Environment&&);
    Environment& operator=(Environment&&);
    ~Environment();

    struct DeclarationInfo {
      std::uint64_t head;
      TypedValue value;
    };
    DeclarationInfo declare_check(std::string_view expr);
    DeclarationInfo declare_check(std::string name, std::string_view expr);
    DeclarationInfo axiom_check(std::string_view expr);
    DeclarationInfo axiom_check(std::string name, std::string_view expr);
    void name_external(std::string name, std::uint64_t external);

    void debug_parse(std::string_view, std::ostream& output = std::cout);
    ParseResult parse(std::string_view);

    Context& context();
    expression::data::SmallScalar<std::uint64_t> const& u64() const;
    expression::data::SmallScalar<imported_type::StringHolder> const& str() const;

  };
}

#endif
