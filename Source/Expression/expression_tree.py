# Source Generator

shape = CompoundShape({
    "Expression": {
        "Apply": [
            ("lhs", "Expression"),
            ("rhs", "Expression")
        ],
        "Arg": [],
        "External": [],
        "Data": []
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
        ]
    }
})
tree_def = TreeOutput(
    trees = [output]
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def,
    relative_includes = ["data.hpp"]
)
