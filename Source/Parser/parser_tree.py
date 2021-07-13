import os;

specification = {
    "shapes": {
        "parser_shape": {
            "components": {
                "Apply": [
                    ("lhs", "Self"),
                    ("rhs", "Self")
                ],
                "Lambda": [
                    ("body", "Self"),
                    ("type", "OptionalSelf")
                ],
                "Identifier": [],
                "Hole": [],
                "Arrow": [
                    ("domain", "Self"),
                    ("codomain", "Self")
                ]
            }
        }
    },
    "paths": {
        "parser::path::Path": "parser_shape"
    },
    "trees": {
        "parser::output::Tree": {
            "shape": "parser_shape",
            "data": {
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
                ]
            }
        },
        "parser::locator::Tree": {
            "shape": "parser_shape",
            "data": {
                "Apply": [
                    ("position", "std::string_view")
                ],
                "Lambda": [
                    ("position", "std::string_view")
                ],
                "Identifier": [
                    ("position", "std::string_view")
                ],
                "Hole": [
                    ("position", "std::string_view")
                ],
                "Arrow": [
                    ("position", "std::string_view")
                ]
            }
        }
    },
    "multitrees": {
        "parser::located_output::Tree": [
            ("parser::output::Tree", "output"),
            ("parser::locator::Tree", "locator")
        ]
    },
    "files": {
        "this.hpp": [
            "parser::path::Path",
            "parser::output::Tree",
            "parser::locator::Tree",
            "parser::located_output::Tree"
        ]
    }
}
