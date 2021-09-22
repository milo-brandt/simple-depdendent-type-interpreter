compiler = g++-10
compiler_cmd = $(compiler) -std=c++20


.PHONY: debug
debug: Debug/program
	gdb -x Tools/gdb_init Debug/program


Debug/program: Debug/Expression/data.o Debug/Expression/stack.o Debug/Compiler/pattern_outline_impl.o Debug/Expression/expression_tree.o Debug/Compiler/instruction_tree_impl.o Debug/Expression/expression_tree_impl.o Debug/main.o Debug/Compiler/pattern_outline.o Debug/ImportedTypes/string_holder.o Debug/Compiler/evaluator.o Debug/Expression/data_pattern_tree_impl.o Debug/Expression/standard_solver_context.o Debug/Expression/expression_debug_format.o Debug/Expression/pattern_tree_impl.o Debug/Expression/formatter.o Debug/Expression/solve_routine.o Debug/Expression/evaluation_context.o Debug/Compiler/instructions.o Debug/ExpressionParser/parser_tree.o Debug/Expression/solver.o Debug/Expression/interactive_environment.o Debug/ExpressionParser/parser_tree_impl.o
	$(compiler_cmd) Debug/Expression/data.o Debug/Expression/stack.o Debug/Compiler/pattern_outline_impl.o Debug/Expression/expression_tree.o Debug/Compiler/instruction_tree_impl.o Debug/Expression/expression_tree_impl.o Debug/main.o Debug/Compiler/pattern_outline.o Debug/ImportedTypes/string_holder.o Debug/Compiler/evaluator.o Debug/Expression/data_pattern_tree_impl.o Debug/Expression/standard_solver_context.o Debug/Expression/expression_debug_format.o Debug/Expression/pattern_tree_impl.o Debug/Expression/formatter.o Debug/Expression/solve_routine.o Debug/Expression/evaluation_context.o Debug/Compiler/instructions.o Debug/ExpressionParser/parser_tree.o Debug/Expression/solver.o Debug/Expression/interactive_environment.o Debug/ExpressionParser/parser_tree_impl.o -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O0 -ggdb -o Debug/program


Debug/Expression/data.o: Source/Expression/data.cpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/expression_tree_impl.inl Source/Expression/pattern_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/function.hpp Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/data.cpp -o Debug/Expression/data.o

Debug/Expression/stack.o: Source/Expression/expression_tree_impl.hpp Source/Expression/stack.cpp Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data.hpp Source/Expression/expression_tree_impl.inl Source/Expression/pattern_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/stack.hpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/function.hpp Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/stack.cpp -o Debug/Expression/stack.o

Debug/Compiler/pattern_outline_impl.o: Source/Utility/tags.hpp Source/Compiler/pattern_outline_impl.cpp Source/Utility/overloaded.hpp Source/Compiler/pattern_outline_impl.hpp Source/Utility/indirect.hpp Source/Compiler/pattern_outline_impl.inl
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/pattern_outline_impl.cpp -o Debug/Compiler/pattern_outline_impl.o

Debug/Expression/expression_tree.o: Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data.hpp Source/Expression/expression_tree_impl.inl Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.cpp Source/Utility/overloaded.hpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Utility/function.hpp Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_tree.cpp -o Debug/Expression/expression_tree.o

Debug/Compiler/instruction_tree_impl.o: Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/ExpressionParser/literals.hpp Source/Utility/result.hpp Source/Compiler/instruction_tree_impl.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/instruction_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/overloaded.hpp Source/Compiler/instruction_tree_impl.cpp Source/Compiler/instruction_tree_explanation.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/ExpressionParser/parser_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/instruction_tree_impl.cpp -o Debug/Compiler/instruction_tree_impl.o

Debug/Expression/expression_tree_impl.o: Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data.hpp Source/Expression/expression_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/expression_tree_impl.cpp Source/Utility/indirect.hpp Source/Utility/function.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_tree_impl.cpp -o Debug/Expression/expression_tree_impl.o

Debug/main.o: Source/Expression/data.hpp Source/Expression/data_helper.hpp Source/Expression/data_pattern_tree_impl.inl Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_debug_format.hpp Source/main.cpp Source/Expression/interactive_environment.hpp Source/Utility/tags.hpp Source/Expression/expression_tree_impl.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Utility/function_info.hpp Source/Utility/function.hpp Source/Expression/expression_tree_impl.inl Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/ImportedTypes/string_holder.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/main.cpp -o Debug/main.o

Debug/Compiler/pattern_outline.o: Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data.hpp Source/Expression/expression_tree_impl.inl Source/Expression/pattern_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/data_pattern_tree_impl.inl Source/Compiler/pattern_outline_impl.hpp Source/Utility/indirect.hpp Source/Compiler/pattern_outline.hpp Source/Compiler/pattern_outline.cpp Source/Expression/evaluation_context.hpp Source/Utility/function.hpp Source/Expression/expression_tree.hpp Source/Expression/pattern_tree_impl.hpp Source/Compiler/pattern_outline_impl.inl
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/pattern_outline.cpp -o Debug/Compiler/pattern_outline.o

Debug/ImportedTypes/string_holder.o: Source/ImportedTypes/string_holder.hpp Source/ImportedTypes/string_holder.cpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/ImportedTypes/string_holder.cpp -o Debug/ImportedTypes/string_holder.o

Debug/Compiler/evaluator.o: Source/ExpressionParser/literals.hpp Source/Expression/data.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/evaluator.hpp Source/Expression/data_pattern_tree_impl.inl Source/Compiler/pattern_outline_impl.hpp Source/Compiler/pattern_outline.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Compiler/evaluator.cpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/instruction_tree_explanation.hpp Source/Utility/function.hpp Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.inl Source/ExpressionParser/parser_tree.hpp Source/Expression/expression_tree_impl.inl Source/Compiler/instruction_tree_impl.hpp Source/Utility/result.hpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Compiler/instruction_tree_impl.inl Source/Expression/stack.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/evaluator.cpp -o Debug/Compiler/evaluator.o

Debug/Expression/data_pattern_tree_impl.o: Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data_pattern_tree_impl.cpp Source/Utility/overloaded.hpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/indirect.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/data_pattern_tree_impl.cpp -o Debug/Expression/data_pattern_tree_impl.o

Debug/Expression/standard_solver_context.o: Source/Expression/data.hpp Source/Expression/data_pattern_tree_impl.inl Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/tags.hpp Source/Expression/solver_context.hpp Source/Expression/expression_tree_impl.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/standard_solver_context.cpp Source/Expression/standard_solver_context.hpp Source/Utility/function.hpp Source/Expression/expression_tree_impl.inl Source/Expression/solver_context_impl.hpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Expression/solver.hpp Source/Expression/stack.hpp Source/Expression/solver_context_interface.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/standard_solver_context.cpp -o Debug/Expression/standard_solver_context.o

Debug/Expression/expression_debug_format.o: Source/Utility/tags.hpp Source/Expression/expression_tree_impl.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data.hpp Source/Expression/expression_tree_impl.inl Source/Expression/pattern_tree_impl.inl Source/Expression/expression_debug_format.cpp Source/Utility/overloaded.hpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/function.hpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/expression_debug_format.hpp Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/expression_debug_format.cpp -o Debug/Expression/expression_debug_format.o

Debug/Expression/pattern_tree_impl.o: Source/Utility/tags.hpp Source/Expression/pattern_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/pattern_tree_impl.cpp Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/pattern_tree_impl.cpp -o Debug/Expression/pattern_tree_impl.o

Debug/Expression/formatter.o: Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data.hpp Source/Expression/formatter.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/formatter.cpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/function.hpp Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/formatter.cpp -o Debug/Expression/formatter.o

Debug/Expression/solve_routine.o: Source/ExpressionParser/literals.hpp Source/Expression/data.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Compiler/evaluator.hpp Source/Expression/data_pattern_tree_impl.inl Source/Compiler/pattern_outline_impl.hpp Source/Expression/pattern_tree_impl.hpp Source/Compiler/pattern_outline.hpp Source/Expression/evaluation_context.hpp Source/Utility/tags.hpp Source/Expression/expression_tree_impl.hpp Source/Expression/solver_context.hpp Source/Expression/data_pattern_tree_impl.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Expression/standard_solver_context.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/function.hpp Source/Expression/solve_routine.cpp Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.inl Source/ExpressionParser/parser_tree.hpp Source/Expression/expression_tree_impl.inl Source/Compiler/instruction_tree_impl.hpp Source/Utility/result.hpp Source/Expression/solver_context_impl.hpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Expression/solve_routine.hpp Source/Expression/solver.hpp Source/Compiler/instruction_tree_impl.inl Source/Expression/stack.hpp Source/Expression/solver_context_interface.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/solve_routine.cpp -o Debug/Expression/solve_routine.o

Debug/Expression/evaluation_context.o: Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/data.hpp Source/Expression/expression_tree_impl.inl Source/Expression/pattern_tree_impl.inl Source/Utility/overloaded.hpp Source/Expression/evaluation_context.cpp Source/Expression/data_pattern_tree_impl.inl Source/Utility/indirect.hpp Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/function.hpp Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/evaluation_context.cpp -o Debug/Expression/evaluation_context.o

Debug/Compiler/instructions.o: Source/Utility/indirect.hpp Source/Utility/tags.hpp Source/ExpressionParser/literals.hpp Source/Utility/result.hpp Source/Compiler/instruction_tree_impl.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Compiler/instruction_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/overloaded.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Compiler/instructions.cpp Source/ExpressionParser/resolution_context_impl.hpp Source/Compiler/instructions.hpp Source/ExpressionParser/parser_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Compiler/instructions.cpp -o Debug/Compiler/instructions.o

Debug/ExpressionParser/parser_tree.o: Source/Utility/indirect.hpp Source/ExpressionParser/parser_tree.cpp Source/Utility/tags.hpp Source/ExpressionParser/literals.hpp Source/Utility/result.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/ExpressionParser/parser_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/ExpressionParser/parser_tree.cpp -o Debug/ExpressionParser/parser_tree.o

Debug/Expression/solver.o: Source/Expression/data.hpp Source/Expression/data_pattern_tree_impl.inl Source/Expression/pattern_tree_impl.hpp Source/Expression/evaluation_context.hpp Source/Utility/tags.hpp Source/Expression/solver_context.hpp Source/Expression/expression_tree_impl.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Utility/function.hpp Source/Expression/expression_tree_impl.inl Source/Expression/solver_context_impl.hpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Expression/solver.hpp Source/Expression/solver.cpp Source/Expression/stack.hpp Source/Expression/solver_context_interface.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/solver.cpp -o Debug/Expression/solver.o

Debug/Expression/interactive_environment.o: Source/ExpressionParser/literals.hpp Source/Expression/data.hpp Source/Parser/parser.hpp Source/ExpressionParser/parser_tree_impl.hpp Source/Expression/data_helper.hpp Source/Compiler/evaluator.hpp Source/Expression/data_pattern_tree_impl.inl Source/Compiler/pattern_outline_impl.hpp Source/Expression/pattern_tree_impl.hpp Source/Compiler/pattern_outline.hpp Source/Expression/evaluation_context.hpp Source/Expression/expression_debug_format.hpp Source/Expression/interactive_environment.hpp Source/Expression/expression_tree_impl.hpp Source/Utility/tags.hpp Source/Expression/solver_context.hpp Source/Expression/data_pattern_tree_impl.hpp Source/Expression/formatter.hpp Source/ExpressionParser/parser_tree_impl.inl Source/Expression/standard_solver_context.hpp Source/Utility/function_info.hpp Source/Compiler/instruction_tree_explanation.hpp Source/Utility/function.hpp Source/CLI/format.hpp Source/Compiler/instructions.hpp Source/Compiler/pattern_outline_impl.inl Source/ExpressionParser/parser_tree.hpp Source/ExpressionParser/expression_parser.hpp Source/Utility/result.hpp Source/Expression/expression_tree_impl.inl Source/Expression/solver_context_impl.hpp Source/Compiler/instruction_tree_impl.hpp Source/Utility/overloaded.hpp Source/Utility/indirect.hpp Source/Expression/solve_routine.hpp Source/Parser/recursive_macros.hpp Source/Expression/solver.hpp Source/Compiler/instruction_tree_impl.inl Source/ImportedTypes/string_holder.hpp Source/Expression/stack.hpp Source/Expression/interactive_environment.cpp Source/Expression/solver_context_interface.hpp Source/ExpressionParser/resolution_context_impl.hpp Source/Expression/pattern_tree_impl.inl Source/Expression/expression_tree.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Expression/interactive_environment.cpp -o Debug/Expression/interactive_environment.o

Debug/ExpressionParser/parser_tree_impl.o: Source/Utility/tags.hpp Source/ExpressionParser/literals.hpp Source/ExpressionParser/parser_tree_impl.inl Source/ExpressionParser/parser_tree_impl.hpp Source/Utility/overloaded.hpp Source/ExpressionParser/parser_tree_impl.cpp Source/Utility/indirect.hpp
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/ExpressionParser/parser_tree_impl.cpp -o Debug/ExpressionParser/parser_tree_impl.o





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
Source/Expression/data_pattern_tree_impl.hpp : Source/Expression/data_pattern_tree.py
	python3 Tools/source_generator.py Source/Expression/data_pattern_tree.py


Source/Expression/data_pattern_tree_impl.cpp : Source/Expression/data_pattern_tree_impl.hpp
Source/Expression/data_pattern_tree_impl.inl : Source/Expression/data_pattern_tree_impl.hpp
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

.PHONY: regenerate
regenerate:
	python3 Tools/makefile_generator.py