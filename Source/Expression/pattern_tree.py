import os;

specification = {
    "shapes": {
        "pattern_shape": {
            "components": {
                "Apply": [
                    ("lhs", "Self"),
                    ("rhs", "Self")
                ],
                "Fixed": [],
                "Wildcard": []
            }
        }
    },
    "paths": {
        "expression::pattern_path::Path": "pattern_shape"
    },
    "trees": {
        "expression::pattern::Tree": {
            "shape": "pattern_shape",
            "path": "expression::pattern_path::Path",
            "data": {
                "Apply": [],
                "Fixed": [
                    ("index", "std::uint64_t")
                ],
                "Wildcard": []
            }
        }
    },
    "files": {
        "this_impl.hpp": [
            "expression::pattern_path::Path",
            "expression::pattern::Tree"
        ]
    }
}
