# Source Generator

shape = CompoundShape({
    "Expression": {
        "Apply": [
            ("lhs", "Expression"),
            ("rhs", "Expression")
        ],
        "Local": [],
        "Embed": [],
        "PrimitiveExpression": [],
        "TypeFamilyOver": [
            ("type", "Expression")
        ]
    },
    "Pattern": {
        "PatternApply": [
            ("lhs", "Pattern"),
            ("rhs", "Pattern")
        ],
        "PatternLocal": [],
        "PatternEmbed": [],
        "PatternCapture": [],
        "PatternHole": []
    },
    "Command": {
        "DeclareHole": [
            ("type", "Expression")
        ],
        "Declare": [
            ("type", "Expression")
        ],
        "Axiom": [
            ("type", "Expression")
        ],
        "Let": [
            ("value", "Expression"),
            ("type", optional("Expression"))
        ],
        "Rule": [
            ("primary_pattern", "Pattern"),
            ("secondary_matches", vector("Expression")),
            ("secondary_patterns", vector("Pattern")),
            ("commands", vector("Command")),
            ("replacement", "Expression")
        ],

    },
    "Program": {
        "ProgramRoot": [
            ("commands", vector("Command")),
            ("value", "Expression")
        ]
    }
})
output = shape.generate_instance(namespace = "compiler::new_instruction::output", data = {
    "Expression": {
        "Apply": [],
        "Local": [
            ("local_index", "std::uint64_t")
        ],
        "Embed": [
            ("embed_index", "std::uint64_t")
        ],
        "PrimitiveExpression": [
            ("primitive", "Primitive")
        ],
        "TypeFamilyOver": []
    },
    "Pattern": {
        "PatternApply": [],
        "PatternLocal": [
            ("local_index", "std::uint64_t")
        ],
        "PatternEmbed": [
            ("embed_index", "std::uint64_t")
        ],
        "PatternCapture": [
            ("capture_index", "std::uint64_t")
        ],
        "PatternHole": []
    },
    "Command": {
        "DeclareHole": [],
        "Declare": [],
        "Axiom": [],
        "Let": [],
        "Rule": [
            ("capture_count", "std::uint64_t")
        ]
    },
    "Program": {
        "ProgramRoot": []
    }
})
locator = shape.generate_instance(namespace = "compiler::new_instruction::locator", data = {
    "Expression": {
        "Apply": [("source", "Explanation")],
        "Local": [("source", "Explanation")],
        "Embed": [("source", "Explanation")],
        "PrimitiveExpression": [("source", "Explanation")],
        "TypeFamilyOver": [("source", "Explanation")]
    },
    "Pattern": {
        "PatternApply": [("source", "Explanation")],
        "PatternLocal": [("source", "Explanation")],
        "PatternEmbed": [("source", "Explanation")],
        "PatternCapture": [("source", "Explanation")],
        "PatternHole": [("source", "Explanation")]
    },
    "Command": {
        "DeclareHole": [("source", "Explanation")],
        "Declare": [("source", "Explanation")],
        "Axiom": [("source", "Explanation")],
        "Let": [("source", "Explanation")],
        "Rule": [("source", "Explanation")]
    },
    "Program": {
        "ProgramRoot": [("source", "Explanation")]
    }
})
forward_locator = shape.generate_instance(namespace = "compiler::new_instruction::forward_locator", data = {
    "Expression": {
        "Apply": [],
        "Local": [],
        "Embed": [],
        "PrimitiveExpression": [],
        "TypeFamilyOver": []
    },
    "Pattern": {
        "PatternApply": [],
        "PatternLocal": [],
        "PatternEmbed": [],
        "PatternCapture": [],
        "PatternHole": []
    },
    "Command": {
        "DeclareHole": [("result", "expression::TypedValue")],
        "Declare": [("result", "expression::TypedValue")],
        "Rule": [],
        "Axiom": [("result", "expression::TypedValue")],
        "Let": [("result", "expression::TypedValue")]
    },
    "Program": {
        "ProgramRoot": []
    }
})
located_output = Multitree("compiler::new_instruction::located_output", {
    "output": output,
    "locator": locator
})
tree_def = TreeOutput(
    trees = [output, locator, forward_locator],
    multitrees = [located_output],
    archive_namespace = "compiler::new_instruction::archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def,
    source_includes = ["Source/ExpressionParser/parser_tree.hpp", "Source/Expression/expression_tree.hpp"],
    relative_includes = ["new_instruction_tree_explanation.hpp"]
)