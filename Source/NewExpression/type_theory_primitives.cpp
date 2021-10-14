#include "type_theory_primitives.hpp"

namespace new_expression {
  TypeTheoryPrimitives::TypeTheoryPrimitives(Arena& arena, RuleCollector& rules):
    type(arena.axiom()),
    arrow(arena.axiom()),
    type_family(arena.declaration()),
    constant(arena.declaration()),
    constant_codomain_1(arena.declaration()),
    constant_codomain_2(arena.declaration()),
    constant_codomain_3(arena.declaration()),
    constant_codomain_4(arena.declaration()),
    id(arena.declaration()),
    id_codomain(arena.declaration()),
    arrow_codomain(arena.declaration()),
    arrow_type(arena.apply(
      arena.copy(arrow),
      arena.copy(type),
      arena.copy(arrow_codomain)
    )) {
    rules.register_declaration(type_family);
    rules.register_declaration(constant);
    rules.register_declaration(constant_codomain_1);
    rules.register_declaration(constant_codomain_2);
    rules.register_declaration(constant_codomain_3);
    rules.register_declaration(constant_codomain_4);
    rules.register_declaration(id);
    rules.register_declaration(id_codomain);
    rules.register_declaration(arrow_codomain);
    rules.add_rule({
      .pattern = lambda_pattern(arena.copy(type_family), 1),
      .replacement = arena.apply(
        arena.copy(arrow),
        arena.argument(0),
        arena.apply(
          arena.copy(constant),
          arena.copy(type),
          arena.copy(type),
          arena.argument(0)
        )
      )
    });
    rules.add_rule({ //constant
      .pattern = lambda_pattern(arena.copy(constant), 4),
      .replacement = arena.argument(1)
    });
    rules.add_rule({ //constant_codomain_1
      .pattern = lambda_pattern(arena.copy(constant_codomain_1), 1),
      .replacement = arena.apply(
        arena.copy(arrow),
        arena.argument(0),
        arena.apply(
          arena.copy(constant_codomain_2),
          arena.argument(0)
        )
      )
    });
    rules.add_rule({ //constant_codomain_2
      .pattern = lambda_pattern(arena.copy(constant_codomain_2), 2),
      .replacement = arena.apply(
        arena.copy(arrow),
        arena.copy(type),
        arena.apply(
          arena.copy(constant_codomain_3),
          arena.argument(0)
        )
      )
    });
    rules.add_rule({ //constant_codomain_3
      .pattern = lambda_pattern(arena.copy(constant_codomain_3), 2),
      .replacement = arena.apply(
        arena.copy(arrow),
        arena.argument(1),
        arena.apply(
          arena.copy(constant),
          arena.copy(type),
          arena.argument(0),
          arena.argument(1)
        )
      )
    });
    rules.add_rule({ //constant_codomain_4
      .pattern = lambda_pattern(arena.copy(constant_codomain_4), 1),
      .replacement = arena.apply(
        arena.copy(type_family),
        arena.copy(type)
      )
    });
    rules.add_rule({ //id
      .pattern = lambda_pattern(arena.copy(id), 2),
      .replacement = arena.argument(1)
    });
    rules.add_rule({ //id_codomain
      .pattern = lambda_pattern(arena.copy(id_codomain), 1),
      .replacement = arena.apply(
        arena.copy(type),
        arena.argument(0),
        arena.apply(
          arena.copy(constant),
          arena.copy(type),
          arena.argument(0),
          arena.argument(0)
        )
      )
    });
    rules.add_rule({ //arrow_codomain
      .pattern = lambda_pattern(arena.copy(arrow_codomain), 1),
      .replacement = arena.apply(
        arena.copy(arrow),
        arena.apply(
          arena.copy(type_family),
          arena.argument(0)
        ),
        arena.apply(
          arena.copy(constant),
          arena.copy(type),
          arena.copy(type),
          arena.apply(
            arena.copy(type_family),
            arena.argument(0)
          )
        )
      )
    });
  }
}
