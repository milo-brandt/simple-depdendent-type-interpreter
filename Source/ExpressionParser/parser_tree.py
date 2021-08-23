# Source Generator

shape = CompoundShape({
    "Expression": {
        "Apply": [
            ("lhs", "Expression"),
            ("rhs", "Expression")
        ],
        "Lambda": [
            ("body", "Expression"),
            ("type", optional("Expression"))
        ],
        "Identifier": [],
        "Hole": [],
        "Arrow": [
            ("domain", "Expression"),
            ("codomain", "Expression")
        ],
        "Block": [
            ("statements", vector("Command")),
            ("value", "Expression")
        ]
    },
    "Pattern": {
        "PatternApply": [
            ("lhs", "Pattern"),
            ("rhs", "Pattern")
        ],
        "PatternIdentifier": [],
        "PatternHole": []
    },
    "Command": {
        "Declare": [
            ("type", "Expression")
        ],
        "Rule": [
            ("pattern", "Pattern"),
            ("replacement", "Expression")
        ],
        "Axiom": [
            ("type", "Expression")
        ],
        "Let": [
            ("value", "Expression"),
            ("type", optional("Expression"))
        ]
    }
})
output = shape.generate_instance(namespace = "expression_parser::output", data = {
    "Expression": {
        "Apply": [],
        "Lambda": [
            ("arg_name", "std::optional<std::string_view>")
        ],
        "Identifier": [
            ("id", "std::string_view")
        ],
        "Hole": [],
        "Arrow": [
            ("arg_name", "std::optional<std::string_view>")
        ],
        "Block": []
    },
    "Pattern": {
        "PatternApply": [],
        "PatternIdentifier": [
            ("id", "std::string_view")
        ],
        "PatternHole": []
    },
    "Command": {
        "Declare": [
            ("name", "std::string_view")
        ],
        "Rule": [],
        "Axiom": [
            ("name", "std::string_view")
        ],
        "Let": [
            ("name", "std::string_view")
        ]
    }
})
locator = shape.generate_instance(namespace = "expression_parser::locator", data = {
    "Expression": {
        "Apply": [
            ("position", "std::string_view")
        ],
        "Lambda": [
            ("position", "std::string_view")
        ],
        "Identifier": [
            ("position", "std::string_view")
        ],
        "Hole": [
            ("position", "std::string_view")
        ],
        "Arrow": [
            ("position", "std::string_view")
        ],
        "Block": [
            ("position", "std::string_view")
        ]
    },
    "Pattern": {
        "PatternApply": [
            ("position", "std::string_view")
        ],
        "PatternIdentifier": [
            ("position", "std::string_view")
        ],
        "PatternHole": [
            ("position", "std::string_view")
        ]
    },
    "Command": {
        "Declare": [
            ("position", "std::string_view")
        ],
        "Rule": [
            ("position", "std::string_view")
        ],
        "Axiom": [
            ("position", "std::string_view")
        ],
        "Let": [
            ("position", "std::string_view")
        ]
    }
})
resolution = shape.generate_instance(namespace = "expression_parser::resolved", data = {
    "Expression": {
        "Apply": [],
        "Lambda": [],
        "Identifier": [
            ("is_local", "bool"),
            ("var_index", "std::uint64_t")
        ],
        "Hole": [],
        "Arrow": [],
        "Block": []
    },
    "Pattern": {
        "PatternApply": [],
        "PatternIdentifier": [
            ("is_local", "bool"),
            ("var_index", "std::uint64_t")
        ],
        "PatternHole": [
            ("var_index", "std::uint64_t") # guarunteed to be local
        ]
    },
    "Command": {
        "Declare": [],
        "Rule": [
            ("args_in_pattern", "std::uint64_t")
        ],
        "Axiom": [],
        "Let": []
    }
})
located_output = Multitree("expression_parser::located_output", {
    "output": output,
    "locator": locator
})
tree_def = TreeOutput(
    trees = [output, locator, resolution],
    multitrees = [located_output],
    archive_namespace = "expression_parser::archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(tree_def)
