# Source Generator

shape = TypeErasedKind(namespace = "expression::solver", name = "Context", functions = [
    MemberFunction("simplify",
        ret="Simplification",
        args=[("tree::Expression","base")]
    ),
    MemberFunction("define_variable",
        ret="void",
        args=[("std::uint64_t","variable"),("std::uint64_t", "args"),("tree::Expression", "body")]
    ),
    #MemberFunction("introduce_variable",
    #    ret="std::uint64_t"
    #),
    MemberFunction("term_depends_on",
        ret="bool",
        args=[("std::uint64_t", "term"), ("std::uint64_t", "possible_dependency")]
    )
])

main_output = get_output("THIS_impl")

main_output.write(
    shape,
    relative_includes=["solver_context_interface.hpp"]
)
