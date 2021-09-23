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
        ],
        "Literal": []
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
        "Block": [],
        "Literal": [
            ("value", "literal::Any")
        ]
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
            ("position", "LexerSpanIndex")
        ],
        "Lambda": [
            ("position", "LexerSpanIndex")
        ],
        "Identifier": [
            ("position", "LexerSpanIndex")
        ],
        "Hole": [
            ("position", "LexerSpanIndex")
        ],
        "Arrow": [
            ("position", "LexerSpanIndex")
        ],
        "Block": [
            ("position", "LexerSpanIndex")
        ],
        "Literal": [
            ("position", "LexerSpanIndex")
        ]
    },
    "Pattern": {
        "PatternApply": [
            ("position", "LexerSpanIndex")
        ],
        "PatternIdentifier": [
            ("position", "LexerSpanIndex")
        ],
        "PatternHole": [
            ("position", "LexerSpanIndex")
        ]
    },
    "Command": {
        "Declare": [
            ("position", "LexerSpanIndex")
        ],
        "Rule": [
            ("position", "LexerSpanIndex")
        ],
        "Axiom": [
            ("position", "LexerSpanIndex")
        ],
        "Let": [
            ("position", "LexerSpanIndex")
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
        "Block": [],
        "Literal": [
            ("embed_index", "std::uint64_t")
        ]
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

main_output.write(
    tree_def,
    relative_includes = ["literals.hpp", "lexer_tree.hpp"]
)
