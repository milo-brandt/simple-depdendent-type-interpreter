#ifndef STANDARD_COMPILER_CONTEXT_HPP
#define STANDARD_COMPILER_CONTEXT_HPP

#include <unordered_map>
#include "compile_stages.hpp"

namespace pipeline::compile {
  struct ModuleInfo {
    new_expression::OwnedExpression module_type;
    new_expression::OwnedExpression full_module_ctor; //String -> Vector ModuleEntry -> Module;
    new_expression::OwnedExpression module_entry_type;
    new_expression::OwnedExpression module_entry_ctor;
    static constexpr auto part_info = mdb::parts::simple<4>;
  };
  struct StandardCompilerContext {
    new_expression::Arena& arena;
    solver::BasicContext context;
    std::unordered_map<std::string, new_expression::TypedValue> names_to_values;
    new_expression::SharedDataTypePointer<primitive::U64Data> u64;
    new_expression::SharedDataTypePointer<primitive::StringData> str;
    new_expression::OwnedExpression vec_type;
    new_expression::OwnedExpression empty_vec;
    new_expression::OwnedExpression cons_vec;
    std::unordered_map<std::string, void*> plugin_data;
    static constexpr auto part_info = mdb::parts::simple<9>;
    StandardCompilerContext(new_expression::Arena& arena);
    ~StandardCompilerContext();

    CombinedContext combined_context();
    [[nodiscard]] ModuleInfo create_module_primitives();
  };
}

#endif
