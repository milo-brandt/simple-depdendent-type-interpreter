#ifndef EXPRESSION_INTERACTIVE_ENVIRONMENT_HPP
#define EXPRESSION_INTERACTIVE_ENVIRONMENT_HPP

#include "evaluation_context.hpp"
#include "data_helper.hpp"

namespace expression::interactive {
  struct StrHolder {
    std::shared_ptr<std::string> data;
  };
  inline bool operator==(StrHolder const& lhs, StrHolder const& rhs) {
    return *lhs.data == *rhs.data;
  }
  inline std::ostream& operator<<(std::ostream& o, StrHolder const& holder) {
    return o << "\"" << *holder.data << "\"";
  }
  class Environment {
    struct Impl;
    std::unique_ptr<Impl> impl;
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

    void debug_parse(std::string_view);

    Context& context();
    expression::data::SmallScalar<std::uint64_t> const& u64() const;
    expression::data::SmallScalar<StrHolder> const& str() const;

  };
}

#endif
