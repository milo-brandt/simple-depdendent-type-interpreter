compiler = g++-10
compiler_cmd = $(compiler) -std=c++20

.PHONY: all
all: Build/program

.PHONY: run
run: Build/program
	Build/program

.PHONY: test
test: Build/test_program
	Build/test_program

.PHONY: debug_test
debug_test: Debug/test_program
	gdb -x Tools/gdb_init Debug/test_program

.PHONY: debug
debug: Debug/program
	gdb -x Tools/gdb_init Debug/program

Build/program: Build/Compiler/pattern_outline_impl.o Build/Expression/expression_tree.o Build/Expression/pattern_tree_impl.o Build/Expression/standard_solver_context.o Build/Expression/evaluation_context.o Build/Compiler/evaluator.o Build/Compiler/instructions.o Build/ExpressionParser/parser_tree_impl.o Build/Expression/solver.o Build/Expression/solve_routine.o Build/Compiler/pattern_outline.o Build/main.o Build/ExpressionParser/parser_tree.o Build/Compiler/instruction_tree_impl.o Build/Expression/expression_debug_format.o Build/Expression/expression_tree_impl.o 
	$(compiler_cmd)  Build/Compiler/pattern_outline_impl.o Build/Expression/expression_tree.o Build/Expression/pattern_tree_impl.o Build/Expression/standard_solver_context.o Build/Expression/evaluation_context.o Build/Compiler/evaluator.o Build/Compiler/instructions.o Build/ExpressionParser/parser_tree_impl.o Build/Expression/solver.o Build/Expression/solve_routine.o Build/Compiler/pattern_outline.o Build/main.o Build/ExpressionParser/parser_tree.o Build/Compiler/instruction_tree_impl.o Build/Expression/expression_debug_format.o Build/Expression/expression_tree_impl.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O3 -o Build/program

Debug/program: Debug/Compiler/pattern_outline_impl.o Debug/Expression/expression_tree.o Debug/Expression/pattern_tree_impl.o Debug/Expression/standard_solver_context.o Debug/Expression/evaluation_context.o Debug/Compiler/evaluator.o Debug/Compiler/instructions.o Debug/ExpressionParser/parser_tree_impl.o Debug/Expression/solver.o Debug/Expression/solve_routine.o Debug/Compiler/pattern_outline.o Debug/main.o Debug/ExpressionParser/parser_tree.o Debug/Compiler/instruction_tree_impl.o Debug/Expression/expression_debug_format.o Debug/Expression/expression_tree_impl.o 
	$(compiler_cmd)  Debug/Compiler/pattern_outline_impl.o Debug/Expression/expression_tree.o Debug/Expression/pattern_tree_impl.o Debug/Expression/standard_solver_context.o Debug/Expression/evaluation_context.o Debug/Compiler/evaluator.o Debug/Compiler/instructions.o Debug/ExpressionParser/parser_tree_impl.o Debug/Expression/solver.o Debug/Expression/solve_routine.o Debug/Compiler/pattern_outline.o Debug/main.o Debug/ExpressionParser/parser_tree.o Debug/Compiler/instruction_tree_impl.o Debug/Expression/expression_debug_format.o Debug/Expression/expression_tree_impl.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O0 -ggdb -o Debug/program

Build/test_program: Build/Tests/combinator_matching.o Build/Expression/pattern_tree_impl.o Build/Expression/expression_tree.o Build/Tests/expression_parser_tests.o Build/ExpressionParser/parser_tree_impl.o Build/ExpressionParser/parser_tree.o Build/Tests/parser_test.o Build/Expression/expression_tree_impl.o Build/Tests/test_main.o 
	$(compiler_cmd)  Build/Tests/combinator_matching.o Build/Expression/pattern_tree_impl.o Build/Expression/expression_tree.o Build/Tests/expression_parser_tests.o Build/ExpressionParser/parser_tree_impl.o Build/ExpressionParser/parser_tree.o Build/Tests/parser_test.o Build/Expression/expression_tree_impl.o Build/Tests/test_main.o  -LDependencies/lib -O3 -o Build/test_program

Debug/test_program: Debug/Tests/combinator_matching.o Debug/Expression/pattern_tree_impl.o Debug/Expression/expression_tree.o Debug/Tests/expression_parser_tests.o Debug/ExpressionParser/parser_tree_impl.o Debug/ExpressionParser/parser_tree.o Debug/Tests/parser_test.o Debug/Expression/expression_tree_impl.o Debug/Tests/test_main.o 
	$(compiler_cmd)  Debug/Tests/combinator_matching.o Debug/Expression/pattern_tree_impl.o Debug/Expression/expression_tree.o Debug/Tests/expression_parser_tests.o Debug/ExpressionParser/parser_tree_impl.o Debug/ExpressionParser/parser_tree.o Debug/Tests/parser_test.o Debug/Expression/expression_tree_impl.o Debug/Tests/test_main.o  -LDependencies/lib -O0 -ggdb -o Debug/test_program


Source/Expression/solver_context_impl.hpp : Source/Expression/solver_context.py
	python3 Tools/source_generator.py Source/Expression/solver_context.py


Source/Expression/expression_tree_impl.hpp : Source/Expression/expression_tree.py
	python3 Tools/source_generator.py Source/Expression/expression_tree.py


Source/Expression/expression_tree_impl.cpp : Source/Expression/expression_tree_impl.hpp
Source/Expression/expression_tree_impl.inl : Source/Expression/expression_tree_impl.hpp
Source/Expression/pattern_tree_impl.hpp : Source/Expression/pattern_tree.py
	python3 Tools/source_generator.py Source/Expression/pattern_tree.py


Source/Expression/pattern_tree_impl.cpp : Source/Expression/pattern_tree_impl.hpp
Source/Expression/pattern_tree_impl.inl : Source/Expression/pattern_tree_impl.hpp
Source/Compiler/instruction_tree_impl.hpp : Source/Compiler/instruction_tree.py
	python3 Tools/source_generator.py Source/Compiler/instruction_tree.py


Source/Compiler/instruction_tree_impl.cpp : Source/Compiler/instruction_tree_impl.hpp
Source/Compiler/instruction_tree_impl.inl : Source/Compiler/instruction_tree_impl.hpp
Source/Compiler/pattern_outline_impl.hpp : Source/Compiler/pattern_outline.py
	python3 Tools/source_generator.py Source/Compiler/pattern_outline.py


Source/Compiler/pattern_outline_impl.cpp : Source/Compiler/pattern_outline_impl.hpp
Source/Compiler/pattern_outline_impl.inl : Source/Compiler/pattern_outline_impl.hpp
Source/ExpressionParser/resolution_context_impl.hpp : Source/ExpressionParser/resolution_context.py
	python3 Tools/source_generator.py Source/ExpressionParser/resolution_context.py


Source/ExpressionParser/parser_tree_impl.hpp : Source/ExpressionParser/parser_tree.py
	python3 Tools/source_generator.py Source/ExpressionParser/parser_tree.py


Source/ExpressionParser/parser_tree_impl.cpp : Source/ExpressionParser/parser_tree_impl.hpp
Source/ExpressionParser/parser_tree_impl.inl : Source/ExpressionParser/parser_tree_impl.hpp


Build/main.o: Source/main.cpp Source/Compiler/instruction_tree_impl.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Expression/solver_context_interface.hpp Source/Utility/function.hpp Source/Expression/solver.hpp Source/Utility/result.hpp Source/ExpressionParser/expression_parser.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/solver_context_impl.hpp Source/Expression/solve_routine.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Expression/standard_solver_context.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Compiler/pattern_outline_impl.inl Source/Parser/parser.hpp Source/Compiler/evaluator.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Compiler/instruction_tree_impl.inl Source/Parser/recursive_macros.hpp Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/expression_debug_format.hpp Source/Expression/solver_context.hpp Source/Expression/expression_tree_impl.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/main.cpp -o Build/main.o

Build/TypeTheory/type_theory.o: Source/TypeTheory/type_theory.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/TypeTheory/type_theory.cpp -o Build/TypeTheory/type_theory.o

Build/Expression/solve_routine.o: Source/Expression/solve_routine.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Compiler/instruction_tree_impl.hpp Source/Expression/standard_solver_context.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Expression/solver_context_interface.hpp Source/Utility/function.hpp Source/Compiler/pattern_outline_impl.inl Source/Compiler/evaluator.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/solver.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/result.hpp Source/Expression/expression_tree_impl.inl Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instruction_tree_impl.inl Source/Compiler/instructions.hpp Source/Expression/solver_context_impl.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/solver_context.hpp Source/Expression/solve_routine.hpp Source/Expression/expression_tree_impl.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/solve_routine.cpp -o Build/Expression/solve_routine.o

Build/Expression/expression_debug_format.o: Source/Expression/expression_debug_format.cpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Utility/function.hpp Source/Expression/expression_debug_format.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/expression_debug_format.cpp -o Build/Expression/expression_debug_format.o

Build/Expression/expression_tree_impl.o: Source/Expression/expression_tree_impl.cpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/expression_tree_impl.cpp -o Build/Expression/expression_tree_impl.o

Build/Expression/expression_tree.o: Source/Expression/expression_tree.cpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/expression_tree.cpp -o Build/Expression/expression_tree.o

Build/Expression/solver.o: Source/Expression/solver.cpp Source/Expression/solver_context.hpp Source/Expression/expression_tree.hpp Source/Expression/evaluation_context.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/solver_context_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/solver_context_interface.hpp Source/Expression/solver.hpp Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/solver.cpp -o Build/Expression/solver.o

Build/Expression/rule_simplification.o: Source/Expression/rule_simplification.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/rule_simplification.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/rule_simplification.cpp -o Build/Expression/rule_simplification.o

Build/Expression/standard_solver_context.o: Source/Expression/standard_solver_context.cpp Source/Expression/solver_context.hpp Source/Expression/solver.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Expression/standard_solver_context.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/solver_context_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/solver_context_interface.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/standard_solver_context.cpp -o Build/Expression/standard_solver_context.o

Build/Expression/pattern_tree_impl.o: Source/Expression/pattern_tree_impl.cpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/pattern_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/pattern_tree_impl.cpp -o Build/Expression/pattern_tree_impl.o

Build/Expression/formatter.o: Source/Expression/formatter.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/formatter.hpp Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/formatter.cpp -o Build/Expression/formatter.o

Build/Expression/evaluation_context.o: Source/Expression/evaluation_context.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/evaluation_context.cpp -o Build/Expression/evaluation_context.o

Build/WebInterface/web_interface.o: Source/WebInterface/web_interface.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/WebInterface/web_interface.cpp -o Build/WebInterface/web_interface.o

Build/WebInterface/session.o: Source/WebInterface/session.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/WebInterface/session.cpp -o Build/WebInterface/session.o

Build/Compiler/pattern_outline.o: Source/Compiler/pattern_outline.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Compiler/pattern_outline_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/pattern_outline.cpp -o Build/Compiler/pattern_outline.o

Build/Compiler/instructions.o: Source/Compiler/instructions.cpp Source/Compiler/instruction_tree_impl.inl Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instructions.hpp Source/Compiler/instruction_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/instructions.cpp -o Build/Compiler/instructions.o

Build/Compiler/evaluator.o: Source/Compiler/evaluator.cpp Source/Compiler/instruction_tree_impl.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/function.hpp Source/Utility/result.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Compiler/pattern_outline_impl.inl Source/Compiler/evaluator.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Compiler/instruction_tree_impl.inl Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/expression_tree_impl.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/evaluator.cpp -o Build/Compiler/evaluator.o

Build/Compiler/pattern_outline_impl.o: Source/Compiler/pattern_outline_impl.cpp Source/Utility/overloaded.hpp Source/Compiler/pattern_outline_impl.inl Source/Utility/indirect.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/pattern_outline_impl.cpp -o Build/Compiler/pattern_outline_impl.o

Build/Compiler/instruction_tree_impl.o: Source/Compiler/instruction_tree_impl.cpp Source/Compiler/instruction_tree_impl.inl Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instruction_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/instruction_tree_impl.cpp -o Build/Compiler/instruction_tree_impl.o

Build/ExpressionParser/parser_tree_impl.o: Source/ExpressionParser/parser_tree_impl.cpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/ExpressionParser/parser_tree_impl.cpp -o Build/ExpressionParser/parser_tree_impl.o

Build/ExpressionParser/parser_tree.o: Source/ExpressionParser/parser_tree.cpp Source/ExpressionParser/resolution_context_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/ExpressionParser/parser_tree.cpp -o Build/ExpressionParser/parser_tree.o

Build/Tests/combinator_matching.o: Source/Tests/combinator_matching.cpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/combinator_matching.cpp -o Build/Tests/combinator_matching.o

Build/Tests/expression_parser_tests.o: Source/Tests/expression_parser_tests.cpp Source/ExpressionParser/resolution_context_impl.hpp Source/Parser/recursive_macros.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Parser/parser.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/expression_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/expression_parser_tests.cpp -o Build/Tests/expression_parser_tests.o

Build/Tests/test_main.o: Source/Tests/test_main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/test_main.cpp -o Build/Tests/test_main.o

Build/Tests/parser_test.o: Source/Tests/parser_test.cpp Source/Parser/recursive_macros.hpp Source/Parser/parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/parser_test.cpp -o Build/Tests/parser_test.o



Debug/main.o: Source/main.cpp Source/Compiler/instruction_tree_impl.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Expression/solver_context_interface.hpp Source/Utility/function.hpp Source/Expression/solver.hpp Source/Utility/result.hpp Source/ExpressionParser/expression_parser.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/solver_context_impl.hpp Source/Expression/solve_routine.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Expression/standard_solver_context.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Compiler/pattern_outline_impl.inl Source/Parser/parser.hpp Source/Compiler/evaluator.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Compiler/instruction_tree_impl.inl Source/Parser/recursive_macros.hpp Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/expression_debug_format.hpp Source/Expression/solver_context.hpp Source/Expression/expression_tree_impl.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/main.cpp -o Debug/main.o

Debug/TypeTheory/type_theory.o: Source/TypeTheory/type_theory.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/TypeTheory/type_theory.cpp -o Debug/TypeTheory/type_theory.o

Debug/Expression/solve_routine.o: Source/Expression/solve_routine.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Compiler/instruction_tree_impl.hpp Source/Expression/standard_solver_context.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Expression/solver_context_interface.hpp Source/Utility/function.hpp Source/Compiler/pattern_outline_impl.inl Source/Compiler/evaluator.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/solver.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/result.hpp Source/Expression/expression_tree_impl.inl Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instruction_tree_impl.inl Source/Compiler/instructions.hpp Source/Expression/solver_context_impl.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/solver_context.hpp Source/Expression/solve_routine.hpp Source/Expression/expression_tree_impl.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/solve_routine.cpp -o Debug/Expression/solve_routine.o

Debug/Expression/expression_debug_format.o: Source/Expression/expression_debug_format.cpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Utility/function.hpp Source/Expression/expression_debug_format.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_debug_format.cpp -o Debug/Expression/expression_debug_format.o

Debug/Expression/expression_tree_impl.o: Source/Expression/expression_tree_impl.cpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_tree_impl.cpp -o Debug/Expression/expression_tree_impl.o

Debug/Expression/expression_tree.o: Source/Expression/expression_tree.cpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_tree.cpp -o Debug/Expression/expression_tree.o

Debug/Expression/solver.o: Source/Expression/solver.cpp Source/Expression/solver_context.hpp Source/Expression/expression_tree.hpp Source/Expression/evaluation_context.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/solver_context_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/solver_context_interface.hpp Source/Expression/solver.hpp Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/solver.cpp -o Debug/Expression/solver.o

Debug/Expression/rule_simplification.o: Source/Expression/rule_simplification.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/rule_simplification.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/rule_simplification.cpp -o Debug/Expression/rule_simplification.o

Debug/Expression/standard_solver_context.o: Source/Expression/standard_solver_context.cpp Source/Expression/solver_context.hpp Source/Expression/solver.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Expression/standard_solver_context.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/solver_context_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/solver_context_interface.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/standard_solver_context.cpp -o Debug/Expression/standard_solver_context.o

Debug/Expression/pattern_tree_impl.o: Source/Expression/pattern_tree_impl.cpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/pattern_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/pattern_tree_impl.cpp -o Debug/Expression/pattern_tree_impl.o

Debug/Expression/formatter.o: Source/Expression/formatter.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/formatter.hpp Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/formatter.cpp -o Debug/Expression/formatter.o

Debug/Expression/evaluation_context.o: Source/Expression/evaluation_context.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/evaluation_context.cpp -o Debug/Expression/evaluation_context.o

Debug/WebInterface/web_interface.o: Source/WebInterface/web_interface.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/WebInterface/web_interface.cpp -o Debug/WebInterface/web_interface.o

Debug/WebInterface/session.o: Source/WebInterface/session.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/WebInterface/session.cpp -o Debug/WebInterface/session.o

Debug/Compiler/pattern_outline.o: Source/Compiler/pattern_outline.cpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Compiler/pattern_outline_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/pattern_outline.cpp -o Debug/Compiler/pattern_outline.o

Debug/Compiler/instructions.o: Source/Compiler/instructions.cpp Source/Compiler/instruction_tree_impl.inl Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instructions.hpp Source/Compiler/instruction_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/instructions.cpp -o Debug/Compiler/instructions.o

Debug/Compiler/evaluator.o: Source/Compiler/evaluator.cpp Source/Compiler/instruction_tree_impl.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/function.hpp Source/Utility/result.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Compiler/pattern_outline_impl.inl Source/Compiler/evaluator.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Compiler/instruction_tree_impl.inl Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/pattern_outline.hpp Source/Expression/expression_tree_impl.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/evaluator.cpp -o Debug/Compiler/evaluator.o

Debug/Compiler/pattern_outline_impl.o: Source/Compiler/pattern_outline_impl.cpp Source/Utility/overloaded.hpp Source/Compiler/pattern_outline_impl.inl Source/Utility/indirect.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/pattern_outline_impl.cpp -o Debug/Compiler/pattern_outline_impl.o

Debug/Compiler/instruction_tree_impl.o: Source/Compiler/instruction_tree_impl.cpp Source/Compiler/instruction_tree_impl.inl Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instruction_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/instruction_tree_impl.cpp -o Debug/Compiler/instruction_tree_impl.o

Debug/ExpressionParser/parser_tree_impl.o: Source/ExpressionParser/parser_tree_impl.cpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/ExpressionParser/parser_tree_impl.cpp -o Debug/ExpressionParser/parser_tree_impl.o

Debug/ExpressionParser/parser_tree.o: Source/ExpressionParser/parser_tree.cpp Source/ExpressionParser/resolution_context_impl.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/ExpressionParser/parser_tree.cpp -o Debug/ExpressionParser/parser_tree.o

Debug/Tests/combinator_matching.o: Source/Tests/combinator_matching.cpp Source/Expression/expression_tree.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/combinator_matching.cpp -o Debug/Tests/combinator_matching.o

Debug/Tests/expression_parser_tests.o: Source/Tests/expression_parser_tests.cpp Source/ExpressionParser/resolution_context_impl.hpp Source/Parser/recursive_macros.hpp Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Parser/parser.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/expression_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/expression_parser_tests.cpp -o Debug/Tests/expression_parser_tests.o

Debug/Tests/test_main.o: Source/Tests/test_main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/test_main.cpp -o Debug/Tests/test_main.o

Debug/Tests/parser_test.o: Source/Tests/parser_test.cpp Source/Parser/recursive_macros.hpp Source/Parser/parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/parser_test.cpp -o Debug/Tests/parser_test.o


.PHONY: clean
clean:
	-rm -r Build
	-rm -r Debug

.PHONY: regenerate
regenerate:
	python3 Tools/makefile_generator.py