# Source Generator

shape = CompoundShape({
    "Term": {
        "Symbol": [],
        "Word": [],
        "StringLiteral": [],
        "IntegerLiteral": [],
        "ParenthesizedExpression": [
            ("body", vector("Term"))
        ],
        "CurlyBraceExpression": [
            ("body", vector("Term"))
        ]
    }
})
output = shape.generate_instance(namespace = "expression_parser::lex_output", data = {
    "Term": {
        "Symbol": [
            ("symbol_index", "std::uint64_t")
        ],
        "Word": [
            ("text", "std::string_view")
        ],
        "StringLiteral": [
            ("text", "std::string")
        ],
        "IntegerLiteral": [
            ("value", "std::uint64_t")
        ],
        "ParenthesizedExpression": [],
        "CurlyBraceExpression": []
    }
})
locator = shape.generate_instance(namespace = "expression_parser::lex_locator", data = {
    "Term": {
        "Symbol": [
            ("position", "std::string_view")
        ],
        "Word": [
            ("position", "std::string_view")
        ],
        "StringLiteral": [
            ("position", "std::string_view")
        ],
        "IntegerLiteral": [
            ("position", "std::string_view")
        ],
        "ParenthesizedExpression": [
            ("position", "std::string_view")
        ],
        "CurlyBraceExpression": [
            ("position", "std::string_view")
        ]
    }
})
located_output = Multitree("expression_parser::lex_located_output", {
    "output": output,
    "locator": locator
})
tree_def = TreeOutput(
    trees = [output, locator],
    multitrees = [located_output],
    archive_namespace = "expression_parser::lex_archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def
)
