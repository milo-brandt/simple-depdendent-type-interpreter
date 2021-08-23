# Source Generator

type_erased_kind = TypeErasedKind("compiler::instruction", "Context", [
    MemberFunction(
        "lookup_external",
        ret = "std::optional<std::size_t>",
        args = [("std::string_view", "name")]
    )
]);
main_output = get_output("THIS_impl")
main_output.write(type_erased_kind)
