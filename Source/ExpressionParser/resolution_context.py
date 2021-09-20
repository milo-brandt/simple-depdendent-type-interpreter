# Source Generator

type_erased_kind = TypeErasedKind("expression_parser::resolved", "Context", [
    MemberFunction(
        "lookup",
        ret = "std::optional<std::uint64_t>",
        args = [("std::string_view", "name")]
    ),
    MemberFunction(
        "embed_literal",
        ret = "std::uint64_t",
        args = [("literal::Any", "value")]
    )
]);
main_output = get_output("THIS_impl")
main_output.write(type_erased_kind)
