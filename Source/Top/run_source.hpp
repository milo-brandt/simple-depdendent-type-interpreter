#include "../Pipeline/standard_compiler_context.hpp"
#include "module_loader.hpp"

mdb::Result<std::pair<pipeline::compile::EvaluateInfo, ModuleLoadInfo>, std::string> run_source(new_expression::Arena& arena, pipeline::compile::StandardCompilerContext& context, std::string_view source);
