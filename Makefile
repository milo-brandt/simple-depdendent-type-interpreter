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
	gdb Debug/test_program

.PHONY: debug
debug: Debug/program
	gdb Debug/program

Build/program: Build/main.o Build/TypeTheory/type_theory.o Build/Expression/expression_tree.o Build/Expression/evaluation_context_interpreter.o Build/Expression/solver_context.o Build/Expression/formatter.o Build/Expression/evaluation_context.o Build/WebInterface/web_interface.o Build/WebInterface/session.o Build/Compiler/resolved_tree.o Build/Compiler/flat_instructions.o 
	$(compiler_cmd)  Build/main.o Build/TypeTheory/type_theory.o Build/Expression/expression_tree.o Build/Expression/evaluation_context_interpreter.o Build/Expression/solver_context.o Build/Expression/formatter.o Build/Expression/evaluation_context.o Build/WebInterface/web_interface.o Build/WebInterface/session.o Build/Compiler/resolved_tree.o Build/Compiler/flat_instructions.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O3 -o Build/program

Debug/program: Debug/main.o Debug/TypeTheory/type_theory.o Debug/Expression/expression_tree.o Debug/Expression/evaluation_context_interpreter.o Debug/Expression/solver_context.o Debug/Expression/formatter.o Debug/Expression/evaluation_context.o Debug/WebInterface/web_interface.o Debug/WebInterface/session.o Debug/Compiler/resolved_tree.o Debug/Compiler/flat_instructions.o 
	$(compiler_cmd)  Debug/main.o Debug/TypeTheory/type_theory.o Debug/Expression/expression_tree.o Debug/Expression/evaluation_context_interpreter.o Debug/Expression/solver_context.o Debug/Expression/formatter.o Debug/Expression/evaluation_context.o Debug/WebInterface/web_interface.o Debug/WebInterface/session.o Debug/Compiler/resolved_tree.o Debug/Compiler/flat_instructions.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O0 -ggdb -o Debug/program

Build/test_program: Build/TypeTheory/type_theory.o Build/Expression/expression_tree.o Build/Expression/evaluation_context_interpreter.o Build/Expression/solver_context.o Build/Expression/formatter.o Build/Expression/evaluation_context.o Build/Compiler/resolved_tree.o Build/Compiler/flat_instructions.o Build/Tests/combinator_matching.o Build/Tests/expression_parser_tests.o Build/Tests/test_main.o Build/Tests/parser_test.o 
	$(compiler_cmd)  Build/TypeTheory/type_theory.o Build/Expression/expression_tree.o Build/Expression/evaluation_context_interpreter.o Build/Expression/solver_context.o Build/Expression/formatter.o Build/Expression/evaluation_context.o Build/Compiler/resolved_tree.o Build/Compiler/flat_instructions.o Build/Tests/combinator_matching.o Build/Tests/expression_parser_tests.o Build/Tests/test_main.o Build/Tests/parser_test.o  -LDependencies/lib -O3 -o Build/test_program

Debug/test_program: Debug/TypeTheory/type_theory.o Debug/Expression/expression_tree.o Debug/Expression/evaluation_context_interpreter.o Debug/Expression/solver_context.o Debug/Expression/formatter.o Debug/Expression/evaluation_context.o Debug/Compiler/resolved_tree.o Debug/Compiler/flat_instructions.o Debug/Tests/combinator_matching.o Debug/Tests/expression_parser_tests.o Debug/Tests/test_main.o Debug/Tests/parser_test.o 
	$(compiler_cmd)  Debug/TypeTheory/type_theory.o Debug/Expression/expression_tree.o Debug/Expression/evaluation_context_interpreter.o Debug/Expression/solver_context.o Debug/Expression/formatter.o Debug/Expression/evaluation_context.o Debug/Compiler/resolved_tree.o Debug/Compiler/flat_instructions.o Debug/Tests/combinator_matching.o Debug/Tests/expression_parser_tests.o Debug/Tests/test_main.o Debug/Tests/parser_test.o  -LDependencies/lib -O0 -ggdb -o Debug/test_program

Source/ExpressionParser/parser_tree_impl.hpp: Source/ExpressionParser/parser_tree.py
	python3 Tools/source_generator.py Source/ExpressionParser/parser_tree.py

Source/Compiler/resolved_tree_impl.hpp: Source/Compiler/resolved_tree.py
	python3 Tools/source_generator.py Source/Compiler/resolved_tree.py

Source/Expression/expression_tree_impl.hpp: Source/Expression/expression_tree.py
	python3 Tools/source_generator.py Source/Expression/expression_tree.py

Source/Expression/pattern_tree_impl.hpp: Source/Expression/pattern_tree.py
	python3 Tools/source_generator.py Source/Expression/pattern_tree.py


Build/main.o: Source/main.cpp Source/Expression/expression_tree.hpp Source/Compiler/resolved_tree_explanation.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context_interpreter.hpp Source/Parser/recursive_macros.hpp Source/WebInterface/web_interface.hpp Source/Compiler/flat_instructions.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Compiler/resolved_tree_impl.hpp Source/ExpressionParser/expression_parser.hpp Source/Expression/solver.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/result.hpp Source/Parser/parser.hpp Source/Expression/solver_context.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/formatter.hpp Source/Expression/evaluation_context.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/main.cpp -o Build/main.o

Build/TypeTheory/type_theory.o: Source/TypeTheory/type_theory.cpp Source/Utility/shared_variant.hpp Source/TypeTheory/type_theory.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/TypeTheory/type_theory.cpp -o Build/TypeTheory/type_theory.o

Build/Expression/expression_tree.o: Source/Expression/expression_tree.cpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/expression_tree.cpp -o Build/Expression/expression_tree.o

Build/Expression/evaluation_context_interpreter.o: Source/Expression/evaluation_context_interpreter.cpp Source/Expression/expression_tree.hpp Source/Compiler/resolved_tree_explanation.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context_interpreter.hpp Source/Compiler/flat_instructions.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/resolved_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/evaluation_context_interpreter.cpp -o Build/Expression/evaluation_context_interpreter.o

Build/Expression/solver_context.o: Source/Expression/solver_context.cpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/solver_context.hpp Source/Expression/evaluation_context.hpp Source/Expression/solver.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/solver_context.cpp -o Build/Expression/solver_context.o

Build/Expression/formatter.o: Source/Expression/formatter.cpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/formatter.hpp Source/Expression/evaluation_context.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/formatter.cpp -o Build/Expression/formatter.o

Build/Expression/evaluation_context.o: Source/Expression/evaluation_context.cpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Expression/evaluation_context.cpp -o Build/Expression/evaluation_context.o

Build/WebInterface/web_interface.o: Source/WebInterface/web_interface.cpp Source/WebInterface/session.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/WebInterface/web_interface.cpp -o Build/WebInterface/web_interface.o

Build/WebInterface/session.o: Source/WebInterface/session.cpp Source/WebInterface/session.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/WebInterface/session.cpp -o Build/WebInterface/session.o

Build/Compiler/resolved_tree.o: Source/Compiler/resolved_tree.cpp Source/Compiler/resolved_tree_explanation.hpp Source/Utility/result.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/resolved_tree_impl.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/resolved_tree.cpp -o Build/Compiler/resolved_tree.o

Build/Compiler/flat_instructions.o: Source/Compiler/flat_instructions.cpp Source/Compiler/resolved_tree_explanation.hpp Source/Compiler/flat_instructions.hpp Source/Utility/result.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/resolved_tree_impl.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Compiler/flat_instructions.cpp -o Build/Compiler/flat_instructions.o

Build/Tests/combinator_matching.o: Source/Tests/combinator_matching.cpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/combinator_matching.cpp -o Build/Tests/combinator_matching.o

Build/Tests/expression_parser_tests.o: Source/Tests/expression_parser_tests.cpp Source/Parser/recursive_macros.hpp Source/Parser/parser.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/ExpressionParser/expression_parser.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/expression_parser_tests.cpp -o Build/Tests/expression_parser_tests.o

Build/Tests/test_main.o: Source/Tests/test_main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/test_main.cpp -o Build/Tests/test_main.o

Build/Tests/parser_test.o: Source/Tests/parser_test.cpp Source/Parser/parser.hpp Source/Parser/recursive_macros.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/parser_test.cpp -o Build/Tests/parser_test.o



Debug/main.o: Source/main.cpp Source/Expression/expression_tree.hpp Source/Compiler/resolved_tree_explanation.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context_interpreter.hpp Source/Parser/recursive_macros.hpp Source/WebInterface/web_interface.hpp Source/Compiler/flat_instructions.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Compiler/resolved_tree_impl.hpp Source/ExpressionParser/expression_parser.hpp Source/Expression/solver.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/result.hpp Source/Parser/parser.hpp Source/Expression/solver_context.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/formatter.hpp Source/Expression/evaluation_context.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/main.cpp -o Debug/main.o

Debug/TypeTheory/type_theory.o: Source/TypeTheory/type_theory.cpp Source/Utility/shared_variant.hpp Source/TypeTheory/type_theory.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/TypeTheory/type_theory.cpp -o Debug/TypeTheory/type_theory.o

Debug/Expression/expression_tree.o: Source/Expression/expression_tree.cpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_tree.cpp -o Debug/Expression/expression_tree.o

Debug/Expression/evaluation_context_interpreter.o: Source/Expression/evaluation_context_interpreter.cpp Source/Expression/expression_tree.hpp Source/Compiler/resolved_tree_explanation.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context_interpreter.hpp Source/Compiler/flat_instructions.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/resolved_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/evaluation_context_interpreter.cpp -o Debug/Expression/evaluation_context_interpreter.o

Debug/Expression/solver_context.o: Source/Expression/solver_context.cpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/solver_context.hpp Source/Expression/evaluation_context.hpp Source/Expression/solver.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/solver_context.cpp -o Debug/Expression/solver_context.o

Debug/Expression/formatter.o: Source/Expression/formatter.cpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/formatter.hpp Source/Expression/evaluation_context.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/formatter.cpp -o Debug/Expression/formatter.o

Debug/Expression/evaluation_context.o: Source/Expression/evaluation_context.cpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/evaluation_context.cpp -o Debug/Expression/evaluation_context.o

Debug/WebInterface/web_interface.o: Source/WebInterface/web_interface.cpp Source/WebInterface/session.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/WebInterface/web_interface.cpp -o Debug/WebInterface/web_interface.o

Debug/WebInterface/session.o: Source/WebInterface/session.cpp Source/WebInterface/session.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/WebInterface/session.cpp -o Debug/WebInterface/session.o

Debug/Compiler/resolved_tree.o: Source/Compiler/resolved_tree.cpp Source/Compiler/resolved_tree_explanation.hpp Source/Utility/result.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/resolved_tree_impl.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/resolved_tree.cpp -o Debug/Compiler/resolved_tree.o

Debug/Compiler/flat_instructions.o: Source/Compiler/flat_instructions.cpp Source/Compiler/resolved_tree_explanation.hpp Source/Compiler/flat_instructions.hpp Source/Utility/result.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/resolved_tree_impl.hpp Source/Compiler/resolved_tree.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/flat_instructions.cpp -o Debug/Compiler/flat_instructions.o

Debug/Tests/combinator_matching.o: Source/Tests/combinator_matching.cpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/combinator_matching.cpp -o Debug/Tests/combinator_matching.o

Debug/Tests/expression_parser_tests.o: Source/Tests/expression_parser_tests.cpp Source/Parser/recursive_macros.hpp Source/Parser/parser.hpp Source/Utility/tags.hpp Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/ExpressionParser/expression_parser.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/expression_parser_tests.cpp -o Debug/Tests/expression_parser_tests.o

Debug/Tests/test_main.o: Source/Tests/test_main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/test_main.cpp -o Debug/Tests/test_main.o

Debug/Tests/parser_test.o: Source/Tests/parser_test.cpp Source/Parser/parser.hpp Source/Parser/recursive_macros.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/parser_test.cpp -o Debug/Tests/parser_test.o


.PHONY: clean
clean:
	-rm -r Build
	-rm -r Debug

.PHONY: regenerate
regenerate:
	python3 Tools/makefile_generator.py