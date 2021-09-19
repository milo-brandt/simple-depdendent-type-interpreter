# Source Generator

shape = CompoundShape({
    "Pattern": {
        "Apply": [
            ("lhs", "Pattern"),
            ("rhs", "Pattern")
        ],
        "Fixed": [],
        "Wildcard": [],
        "Data": []
    }
})
output = shape.generate_instance(namespace = "expression::data_pattern", data = {
    "Pattern": {
        "Apply": [],
        "Fixed": [
            ("external_index", "std::uint64_t")
        ],
        "Wildcard": [],
        "Data": [
            ("type_index", "std::uint64_t")
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
