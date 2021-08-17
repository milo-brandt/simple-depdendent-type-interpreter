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
        "expression_parser::path::Path": "parser_shape"
    },
    "trees": {
        "expression_parser::output::Tree": {
            "shape": "parser_shape",
            "path": "expression_parser::path::Path",
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
        "expression_parser::locator::Tree": {
            "shape": "parser_shape",
            "path": "expression_parser::path::Path",
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
        "expression_parser::located_output::Tree": [
            ("expression_parser::output::Tree", "output"),
            ("expression_parser::locator::Tree", "locator")
        ]
    },
    "files": {
        "this_impl.hpp": [
            "expression_parser::path::Path",
            "expression_parser::output::Tree",
            "expression_parser::locator::Tree",
            "expression_parser::located_output::Tree"
        ]
    }
}
