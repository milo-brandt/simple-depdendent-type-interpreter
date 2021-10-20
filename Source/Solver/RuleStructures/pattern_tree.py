# Source Generator

expr_shape = CompoundShape({
    "PatternExpr": {
        "Apply": [
            ("lhs", "PatternExpr"),
            ("rhs", "PatternExpr")
        ],
        "Embed": [],
        "Capture": [],
        "Hole": []
    }
})
expr = expr_shape.generate_instance(namespace = "solver::pattern_expr", data = {
    "PatternExpr": {
        "Apply": [],
        "Embed": [
            ("value", "new_expression::OwnedExpression")
        ],
        "Capture": [
            ("capture_index", "std::uint64_t")
        ],
        "Hole": []
    }
}, copyable = False);
resolution_locator = expr_shape.generate_instance(namespace = "solver::pattern_resolution_locator", data = {
    "PatternExpr": {
        "Apply": [
            ("source", "compiler::new_instruction::archive_index::PatternApply")
        ],
        "Embed": [
            ("source", "compiler::new_instruction::archive_index::Pattern")
        ],
        "Capture": [
            ("source", "compiler::new_instruction::archive_index::PatternCapture")
        ],
        "Hole": [
            ("source", "compiler::new_instruction::archive_index::PatternHole")
        ]
    }
})
expr_located_resolution = Multitree("solver::pattern_located_resolution", {
    "output": expr,
    "locator": resolution_locator
})
node_shape = CompoundShape({
    "PatternNode": {
        "Apply": [
            ("args", vector("PatternNode"))
        ],
        "Capture": [],
        "Check": [],
        "Ignore": []
    }
})
node = node_shape.generate_instance(namespace = "solver::pattern_node", data = {
    "PatternNode": {
        "Apply": [
            ("head", "new_expression::OwnedExpression")
        ],
        "Capture": [
            ("capture_index", "std::uint64_t")
        ],
        "Check": [
            ("desired_equality", "pattern_expr::PatternExpr")
        ],
        "Ignore": []
    }
}, copyable = False)
node_normalization_locator = node_shape.generate_instance(namespace = "solver::pattern_normalization_locator", data = {
    "PatternNode": {
        "Apply": [
            ("source", "pattern_expr_archive_index::PatternExpr")
        ],
        "Capture": [
            ("source", "pattern_expr_archive_index::Capture")
        ],
        "Check": [
            ("source", "pattern_expr_archive_index::PatternExpr")
        ],
        "Ignore": [
            ("source", "pattern_expr_archive_index::Hole")
        ]
    }
})
node_located_normalization = Multitree("solver::pattern_located_normalization", {
    "output": node,
    "locator": node_normalization_locator
})
expr_def = TreeOutput(
    trees = [expr, resolution_locator],
    multitrees = [expr_located_resolution],
    archive_namespace = "solver::pattern_expr_archive_index"
)
node_def = TreeOutput(
    trees = [node, node_normalization_locator],
    multitrees = [node_located_normalization],
    archive_namespace = "solver::pattern_node_archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(
    expr_def, node_def,
    source_includes = ["Source/NewExpression/arena.hpp", "Source/Compiler/new_instructions.hpp"]
)
