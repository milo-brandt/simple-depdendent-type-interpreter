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
indexed_output = shape.generate_instance(namespace = "expression::indexed_data_pattern", data = {
    "Pattern": {
        "Apply": [],
        "Fixed": [
            ("external_index", "std::uint64_t")
        ],
        "Wildcard": [
            ("match_index", "std::uint64_t")
        ],
        "Data": [
            ("type_index", "std::uint64_t"),
            ("match_index", "std::uint64_t")
        ]
    }
})
tree_def = TreeOutput(
    trees = [output, indexed_output]
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def
)
