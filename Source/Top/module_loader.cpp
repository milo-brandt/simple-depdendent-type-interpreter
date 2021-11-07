#include "module_loader.hpp"
#include <fstream>
#include "../NewExpression/arena_utility.hpp"
#include <dlfcn.h>

bool ModuleLoadInfo::prepare_module(std::string module_name, std::string source, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives) {
  if(module_info.contains(module_name)) return true;
  if(modules_loading.contains(module_name)) {
    std::cout << "Cyclic dependency in modules. Backtrace:\n";
    bool found = false;
    for(auto const& name : module_loading_stack) {
      found = found || name == module_name;
      if(found) {
        std::cout << name << "\n";
      }
    }
    modules_loading.clear();
    module_loading_stack.clear();
    return false;
  }
  modules_loading.insert(module_name);
  module_loading_stack.push_back(module_name);
  auto parsed_module_result = bind(
    pipeline::compile::lex({context.arena, source}),
    [&](auto lexed) {
      return parse_module(std::move(lexed));
    });
  if(parsed_module_result.holds_error()) {
    std::cout << parsed_module_result.get_error() << "\n";
    return false;
  }
  auto& parsed_module = parsed_module_result.get_value();
  std::unordered_map<std::string, new_expression::TypedValue> names_to_values;
  new_expression::RAIIDestroyer destroyer{context.arena, names_to_values};
  for(auto const& entry : context.names_to_values) {
    names_to_values.insert(std::make_pair(entry.first, copy_on_arena(context.arena, entry.second)));
  }
  resolve_imports(parsed_module.first, names_to_values, context, module_primitives);
  auto compilation_result = map(
    resolve(std::move(parsed_module.second), {
      .names_to_values = names_to_values,
      .u64 = context.u64.get(),
      .str = context.str.get(),
      .empty_vec = new_expression::TypedValue{
        .value = context.arena.copy(context.empty_vec),
        .type = context.arena.copy(context.context.type_collector.get_type_of(context.empty_vec))
      },
      .cons_vec = new_expression::TypedValue{
        .value = context.arena.copy(context.cons_vec),
        .type = context.arena.copy(context.context.type_collector.get_type_of(context.cons_vec))
      }
    }),
    [&](auto resolved) {
      return evaluate(std::move(resolved), {
        .context = context.context
      });
    }
  );
  if(compilation_result.holds_error()) {
    std::cout << compilation_result.get_error() << "\n";
    return false;
  }
  auto& compilation = compilation_result.get_value();
  if(!compilation.is_okay()) {
    new_expression::WeakKeyMap<std::string> externals_to_names(context.arena);
    compilation.report_errors_to(std::cout, externals_to_names);
    return false;
  }
  new_expression::EvaluationContext ctx{context.arena, context.context.rule_collector};
  compilation.result.value = ctx.reduce(std::move(compilation.result.value));
  compilation.result.type = ctx.reduce(std::move(compilation.result.type));
  if(compilation.is_okay() && compilation.result.type == module_primitives.module_type) {
    std::unordered_map<std::string, new_expression::TypedValue> exported_terms;
    auto outer_unfolded = unfold(context.arena, compilation.result.value);
    auto load_name = std::string{context.str->read_data(context.arena.get_data(outer_unfolded.args[0])).get_string()};
    new_expression::WeakExpression module_head = outer_unfolded.args[1];
    std::vector<new_expression::TypedValue> ordered_exports;
    while(true) {
      auto unfolded = unfold(context.arena, module_head);
      if(unfolded.args.size() != 3) break;
      auto entry = unfolded.args[1]; //head;
      auto entry_unfolded = unfold(context.arena, entry);
      if(entry_unfolded.args.size() != 3) break; //???
      auto str = std::string{context.str->read_data(context.arena.get_data(entry_unfolded.args[1])).get_string()};
      auto type = entry_unfolded.args[0];
      auto value = entry_unfolded.args[2];
      module_head = unfolded.args[2];
      if(exported_terms.contains(str)) {
        std::cout << "Duplicated export " << str << " in module " << module_name << "\n";
        destroy_from_arena(context.arena, exported_terms, compilation);
        return false;
      }
      auto term = new_expression::TypedValue{
        .value = context.arena.copy(value),
        .type = context.arena.copy(type)
      };
      if(!str.empty()) {
        exported_terms.insert(std::make_pair(std::move(str), copy_on_arena(context.arena, term)));
      } //don't export empty names
      ordered_exports.push_back(std::move(term));
    }
    if(!load_name.empty()) {
      auto path = "./Plugin/" + load_name;
      auto lib = dlopen(path.c_str(), RTLD_LAZY);
      if(!lib) {
        std::cout << "Failed to open shared library at " << path << "\n";
        return false;
      }
      auto sym = dlsym(lib, "initialize");
      if(!sym) {
        std::cout << "Failed to get 'initialize' from shared library.\n";
        return false;
      }
      auto initialize = (void(*)(pipeline::compile::StandardCompilerContext*, new_expression::TypedValue*, new_expression::TypedValue*))sym;
      initialize(&context, ordered_exports.data(), ordered_exports.data() + ordered_exports.size());

      close_actions.push_back([lib, &context] {
        auto sym = dlsym(lib, "cleanup");
        if(sym) {
          auto cleanup = (void(*)(pipeline::compile::StandardCompilerContext*))sym;
          cleanup(&context);
        }
        //dlclose(lib); still apparently not safe to close the library since its functions might be needed to destroy Data entries
      });
      //Note: passed span will be kept alive while module is alive
      //dlclose(lib);
    }
    module_info.insert(std::make_pair(module_name, ModuleInfo{
      .exported_terms = std::move(exported_terms),
      .export_list = std::move(ordered_exports),
      .evaluate_info = std::move(compilation),
      .source = std::move(source)
    }));
    destroy_from_arena(context.arena, ordered_exports);
    modules_loading.erase(module_name);
    module_loading_stack.pop_back();
    return true;
  } else {
    std::cout << "Module " << module_name << " did not return a module object.\n";
    destroy_from_arena(context.arena, compilation);
    return false;
  }
}
bool ModuleLoadInfo::locate_and_prepare_module(std::string module_name, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives) {
  if(module_info.contains(module_name)) return true;
  std::string source;
  std::ifstream f("InnerLib/" + module_name);
  if(!f) {
    std::cout << "Failed to read file \"" << ("InnerLib/" + module_name) << "\"\n";
    return false;
  } else {
    std::getline(f, source, '\0'); //just read the whole file - assuming no null characters in it :P
  }
  return prepare_module(module_name, std::move(source), context, module_primitives);
}
void ModuleLoadInfo::close() {
  for(auto it  = close_actions.rbegin(); it != close_actions.rend(); ++it) {
    (*it)();
  }
  close_actions.clear();
}
bool ModuleLoadInfo::resolve_imports(expression_parser::ModuleHeader const& header, std::unordered_map<std::string, new_expression::TypedValue>& out, pipeline::compile::StandardCompilerContext& context, pipeline::compile::ModuleInfo const& module_primitives) {
  auto add_name = [&](std::string name, new_expression::TypedValue value) {
    if(out.contains(name)) {
      destroy_from_arena(context.arena, value);
      //std::cout << "Duplicated name " << name << " in imports of " << module_name << ".\n";
      return false;
    } else {
      out.insert(std::make_pair(std::move(name), std::move(value)));
      return true;
    }
  };
  for(auto const& import_info : header.imports) {
    if(!locate_and_prepare_module(import_info.module_name, context, module_primitives)) return false;
    auto const& info = module_info.at(import_info.module_name);
    if(import_info.request_all) {
      for(auto const& entry : info.exported_terms) {
        if(!add_name(entry.first, copy_on_arena(context.arena, entry.second))) return false;
      }
    } else {
      for(auto const& request : import_info.requested_names) {
        if(!info.exported_terms.contains(request)) {
          //std::cout << "No name " << request << " in module " << import_info.module_name << " requested from " << module_name << ".\n";
          return false;
        }
        if(!add_name(request, copy_on_arena(context.arena, info.exported_terms.at(request)))) return false;
      }
    }
  }
  return true;
}
