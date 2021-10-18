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
expr_def = TreeOutput(
    trees = [expr]
)
node_def = TreeOutput(
    trees = [node]
)

main_output = get_output("THIS_impl")

main_output.write(
    expr_def, node_def,
    source_includes = ["Source/NewExpression/arena.hpp"]
)
