#ifndef EXPRESSION_NEW_INTERACTIVE_ENVIRONMENT_HPP
#define EXPRESSION_NEW_INTERACTIVE_ENVIRONMENT_HPP

#include <string>
#include <memory>
#include <iostream>
#include "../Pipeline/pipeline.hpp"
#include "../Utility/result.hpp"
#include "../Primitives/primitives.hpp"

namespace interactive {
  class Environment {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Environment();
    Environment(Environment&&);
    Environment& operator=(Environment&&);
    ~Environment();

    void debug_parse(std::string_view, std::ostream& output = std::cout);
    mdb::Result<pipeline::compile::EvaluateInfo, std::string> full_compile(std::string_view);
    new_expression::OwnedExpression declare(std::string, std::string_view);
    new_expression::OwnedExpression axiom(std::string, std::string_view);
    new_expression::Arena& arena();
    solver::BasicContext& context();
    new_expression::WeakExpression u64_head();
    new_expression::WeakExpression u64_type();
    primitive::U64Data* u64();
    new_expression::WeakKeyMap<std::string>& externals_to_names();

  };
}

#endif
