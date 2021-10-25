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
        "VectorLiteral": [
            ("elements", vector("Expression"))
        ],
        "Literal": [],
        "Match": [
            ("matched_expression", "Expression"),
            ("output_type", optional("Expression")),
            ("arm_patterns", vector("Pattern")),
            ("arm_expressions", vector("Expression"))
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
            ("subclause_expressions", vector("Expression")),
            ("subclause_patterns", vector("Pattern")),
            ("replacement", "Expression")
        ],
        "Axiom": [
            ("type", "Expression")
        ],
        "Check": [
            ("term", "Expression"),
            ("expected", optional("Expression")),
            ("expected_type", optional("Expression"))
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
        "VectorLiteral": [],
        "Literal": [
            ("value", "literal::Any")
        ],
        "Match": []
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
        "Check": [
            ("allow_deduction", "bool")
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
        "VectorLiteral": [
            ("position", "LexerSpanIndex")
        ],
        "Literal": [
            ("position", "LexerSpanIndex")
        ],
        "Match": [
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
        "Check": [
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
        "VectorLiteral": [],
        "Literal": [
            ("embed_index", "std::uint64_t")
        ],
        "Match": [
            ("args_in_arm", "std::vector<std::uint64_t>")
        ]
    },
    "Pattern": {
        "PatternApply": [],
        "PatternIdentifier": [
            ("is_local", "bool"),
            ("var_index", "std::uint64_t")
        ],
        "PatternHole": []
    },
    "Command": {
        "Declare": [],
        "Rule": [
            ("captures_used_in_subclause_expression", "std::vector<std::vector<std::uint64_t> >"),
            ("args_in_pattern", "std::uint64_t")
        ],
        "Axiom": [],
        "Check": [
            ("allow_deduction", "bool")
        ],
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
