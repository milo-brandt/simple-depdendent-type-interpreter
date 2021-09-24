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
    "Command": {
        "DeclareHole": [
            ("type", "Expression")
        ],
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
            ("value", "Expression"),
            ("type", optional("Expression"))
        ],
        "ForAll": [
            ("type", "Expression"),
            ("commands", vector("Command"))
        ]
    },
    "Program": {
        "ProgramRoot": [
            ("commands", vector("Command")),
            ("value", "Expression")
        ]
    }
})
output = shape.generate_instance(namespace = "compiler::instruction::output", data = {
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
    "Command": {
        "DeclareHole": [],
        "Declare": [],
        "Rule": [],
        "Axiom": [],
        "Let": [],
        "ForAll": []
    },
    "Program": {
        "ProgramRoot": []
    }
})
locator = shape.generate_instance(namespace = "compiler::instruction::locator", data = {
    "Expression": {
        "Apply": [("source", "Explanation")],
        "Local": [("source", "Explanation")],
        "Embed": [("source", "Explanation")],
        "PrimitiveExpression": [("source", "Explanation")],
        "TypeFamilyOver": [("source", "Explanation")]
    },
    "Command": {
        "DeclareHole": [("source", "Explanation")],
        "Declare": [("source", "Explanation")],
        "Rule": [("source", "Explanation")],
        "Axiom": [("source", "Explanation")],
        "Let": [("source", "Explanation")],
        "ForAll": [("source", "Explanation")]
    },
    "Program": {
        "ProgramRoot": [("source", "Explanation")]
    }
})
forward_locator = shape.generate_instance(namespace = "compiler::instruction::forward_locator", data = {
    "Expression": {
        "Apply": [],
        "Local": [],
        "Embed": [],
        "PrimitiveExpression": [],
        "TypeFamilyOver": []
    },
    "Command": {
        "DeclareHole": [("result", "expression::TypedValue")],
        "Declare": [("result", "expression::TypedValue")],
        "Rule": [],
        "Axiom": [("result", "expression::TypedValue")],
        "Let": [("result", "expression::TypedValue")],
        "ForAll": []
    },
    "Program": {
        "ProgramRoot": []
    }
})
located_output = Multitree("compiler::instruction::located_output", {
    "output": output,
    "locator": locator
})
tree_def = TreeOutput(
    trees = [output, locator, forward_locator],
    multitrees = [located_output],
    archive_namespace = "compiler::instruction::archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def,
    source_includes = ["Source/ExpressionParser/parser_tree.hpp", "Source/Expression/expression_tree.hpp"],
    relative_includes = ["instruction_tree_explanation.hpp"]
)
