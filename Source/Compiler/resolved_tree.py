import os;

specification = {
    "includes": [
        "resolved_tree_explanation.hpp"
    ],
    "shapes": {
        "resolution_shape": {
            "components": {
                "Apply": [
                    ("lhs", "Self"),
                    ("rhs", "Self")
                ],
                "Lambda": [
                    ("body", "Self"),
                    ("type", "Self")
                ],
                "Local": [],
                "Hole": [],
                "Embed": []
            }
        }
    },
    "paths": {
        "compiler::resolution::path::Path": "resolution_shape"
    },
    "trees": {
        "compiler::resolution::output::Tree": {
            "shape": "resolution_shape",
            "path": "compiler::resolution::path::Path",
            "data": {
                "Apply": [],
                "Lambda": [],
                "Local": [
                    ("stack_index", "std::uint64_t")
                ],
                "Hole": [],
                "Embed": [
                    ("embed_index", "std::uint64_t")
                ]
            }
        },
        "compiler::resolution::locator::Tree": {
            "shape": "resolution_shape",
            "path": "compiler::resolution::path::Path",
            "data": {
                "Apply": [
                    ("relative_position", "relative::Position")
                ],
                "Lambda": [
                    ("relative_position", "relative::Position")
                ],
                "Local": [
                    ("relative_position", "relative::Position")
                ],
                "Hole": [
                    ("relative_position", "relative::Position")
                ],
                "Embed": [
                    ("relative_position", "relative::Position")
                ]
            }
        }
    },
    "multitrees": {
        "compiler::resolution::located_output::Tree": [
            ("compiler::resolution::output::Tree", "output"),
            ("compiler::resolution::locator::Tree", "locator")
        ]
    },
    "files": {
        "this_impl.hpp": [
            "compiler::resolution::path::Path",
            "compiler::resolution::output::Tree",
            "compiler::resolution::locator::Tree",
            "compiler::resolution::located_output::Tree"
        ]
    }
}
