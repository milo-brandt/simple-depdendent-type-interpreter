#ifndef RUN_SOURCE_HPP
#define RUN_SOURCE_HPP

#include "../ExpressionParser/module_header.hpp"
#include "../Pipeline/standard_compiler_context.hpp"

struct ModuleInfo {
  std::unordered_map<std::string, new_expression::TypedValue> exported_terms;
  std::vector<new_expression::TypedValue> export_list;
  pipeline::compile::EvaluateInfo evaluate_info;
  std::string source;
  static constexpr auto part_info = mdb::parts::simple<4>;
};
struct ModuleLoadInfo {
  std::unordered_set<std::string> modules_loading;
  std::vector<std::string> module_loading_stack;
  std::unordered_map<std::string, ModuleInfo> module_info;
  std::vector<mdb::function<void()> > close_actions;
  static constexpr auto part_info = mdb::parts::simple<4>;
  bool locate_and_prepare_module(std::string module_name, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives);
  bool prepare_module(std::string module_name, std::string source, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives);
  void close();
  bool resolve_imports(expression_parser::ModuleHeader const&, std::unordered_map<std::string, new_expression::TypedValue>&, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives);
};

#endif
