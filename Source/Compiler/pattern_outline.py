# Source Generator

shape = CompoundShape({
    "Pattern": {
        "Segment": [
            ("args", vector("Pattern"))
        ],
        "CapturePoint": []
    }
})
output = shape.generate_instance(namespace = "compiler::pattern", data = {
    "Pattern": {
        "Segment": [
            ("head", "std::uint64_t")
        ],
        "CapturePoint": []
    }
})
tree_def = TreeOutput(
    trees = [output],
    archive_namespace = "compiler::pattern::archive_index"
)

main_output = get_output("THIS_impl")

main_output.write(
    tree_def
)
