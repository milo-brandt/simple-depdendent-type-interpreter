# Source Generator

shape = CompoundShape({
    "Expression": {
        "Apply": [
            ("lhs", "Expression"),
            ("rhs", "Expression")
        ],
        "Arg": [],
        "External": [],
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
        ]
    }
})
tree_def = TreeOutput(
    trees = [output]
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def
)
