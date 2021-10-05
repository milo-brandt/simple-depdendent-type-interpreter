# Source Generator

shape = CompoundShape({
    "Expression": {
        "Apply": [
            ("lhs", "Expression"),
            ("rhs", "Expression")
        ],
        "Arg": [],
        "External": [],
        "Data": [],
        "Conglomerate": []
    }
})
output = shape.generate_instance(namespace = "expression::tree", data = {
    "Expression": {
        "Apply": [],
        "Arg": [
            ("arg_index", "std::uint64_t")
        ],
        "External": [
            ("external_index", "std::uint64_t")
        ],
        "Data": [
            ("data", "data::Data")
        ],
        "Conglomerate": [
            ("conglomerate_index", "std::uint64_t")
        ]
    }
})
tree_def = TreeOutput(
    trees = [output],
    shared = True
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def,
    relative_includes = ["data.hpp"]
)
