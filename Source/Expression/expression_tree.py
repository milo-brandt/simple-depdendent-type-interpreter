import os;

specification = {
    "shapes": {
        "expression_shape": {
            "components": {
                "Apply": [
                    ("lhs", "Self"),
                    ("rhs", "Self")
                ],
                "External": [],
                "Arg": []
            }
        }
    },
    "paths": {
        "expression::path::Path": "expression_shape"
    },
    "trees": {
        "expression::tree::Tree": {
            "shape": "expression_shape",
            "path": "expression::path::Path",
            "data": {
                "Apply": [],
                "External": [
                    ("index", "std::uint64_t")
                ],
                "Arg": [
                    ("index", "std::uint64_t")
                ]
            }
        }
    },
    "files": {
        "this_impl.hpp": [
            "expression::path::Path",
            "expression::tree::Tree"
        ]
    }
}
