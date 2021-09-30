# Source Generator

async_generic = AsyncGeneric("compiler::async", "GenericInterpreter", "GenericRoutine", ["request::Solve", "request::DefineVariable"]);
main_output = get_output("THIS_impl")
main_output.write(async_generic, relative_includes = ["requests.hpp"])
