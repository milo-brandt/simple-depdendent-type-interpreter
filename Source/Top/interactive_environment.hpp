#ifndef EXPRESSION_NEW_INTERACTIVE_ENVIRONMENT_HPP
#define EXPRESSION_NEW_INTERACTIVE_ENVIRONMENT_HPP

#include <string>
#include <memory>
#include <iostream>
#include "../Pipeline/compile_outputs.hpp"
#include "../Utility/result.hpp"
#include "../Primitives/primitives.hpp"
#include "../NewExpression/arena.hpp"

namespace interactive {
  class Environment {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    Environment(new_expression::Arena&);
    Environment(Environment&&);
    Environment& operator=(Environment&&);
    ~Environment();

    mdb::Result<pipeline::compile::EvaluateInfo, std::string> full_compile(std::string_view);
    new_expression::OwnedExpression declare(std::string, std::string_view);
    new_expression::OwnedExpression axiom(std::string, std::string_view);
    new_expression::Arena& arena();
    solver::BasicContext& context();
    new_expression::WeakExpression u64_type();
    primitive::U64Data* u64();
    new_expression::WeakExpression str_type();
    primitive::StringData* str();
    new_expression::WeakKeyMap<std::string>& externals_to_names();
    new_expression::WeakExpression vec_type();
    void name_primitive(std::string name, new_expression::WeakExpression primitive);
  };
}

#endif
