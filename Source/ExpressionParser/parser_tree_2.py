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
    "Command": {
        "Declare": [
            ("type", "Expression")
        ],
        "Rule": [
            ("pattern", "Expression"),
            ("replacement", "Expression")
        ],
        "Axiom": [
            ("type", "Expression")
        ],
        "Let": [
            ("value", "Expression")
        ]
    }
})
output = shape.generate_instance(namespace = "expression::output", data = {
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
locator = shape.generate_instance(namespace = "expression::locator", data = {
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
located_output = Multitree("expression::located_output", {
    "output": output,
    "locator": locator
})
tree_def = TreeOutput(
    trees = [output, locator],
    multitrees = [located_output],
    archive_namespace = "expression::archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(tree_def)
