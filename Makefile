compiler = g++-10
compiler_cmd = $(compiler) -std=c++20 -fcoroutines -g -fconcepts-diagnostics-depth=10 -Wreturn-type

.PHONY: all
all: Build/program

.PHONY: run
run: Build/program
	Build/program

.PHONY: test
test: Build/test_program
	Build/test_program

.PHONY: debug
debug: Debug/program
	gdb Debug/program

Build/program: Build/main.o Build/expression.o Build/expression_parser.o Build/combinatorial_parser.o 
	$(compiler_cmd)  Build/main.o Build/expression.o Build/expression_parser.o Build/combinatorial_parser.o  -LDependencies/lib -O3 -o Build/program

Debug/program: Debug/main.o Debug/expression.o Debug/expression_parser.o Debug/combinatorial_parser.o 
	$(compiler_cmd)  Debug/main.o Debug/expression.o Debug/expression_parser.o Debug/combinatorial_parser.o  -LDependencies/lib -O0 -ggdb -o Debug/program

Build/test_program: Build/expression.o Build/expression_parser.o Build/combinatorial_parser.o 
	$(compiler_cmd)  Build/expression.o Build/expression_parser.o Build/combinatorial_parser.o  -LDependencies/lib -O3 -o Build/test_program


Build/main.o: Source/main.cpp Source/template_utility.hpp Source/combinatorial_parser.hpp Source/coro_base.hpp Source/lifted_state_machine.hpp Source/operator_tree.hpp Source/indirect_value.hpp Source/expression.hpp Source/expression_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/main.cpp -o Build/main.o

Build/expression.o: Source/expression.cpp Source/template_utility.hpp Source/expression.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/expression.cpp -o Build/expression.o

Build/expression_parser.o: Source/expression_parser.cpp Source/template_utility.hpp Source/combinatorial_parser.hpp Source/operator_tree_parser.hpp Source/operator_tree.hpp Source/expression_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/expression_parser.cpp -o Build/expression_parser.o

Build/combinatorial_parser.o: Source/combinatorial_parser.cpp Source/template_utility.hpp Source/combinatorial_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/combinatorial_parser.cpp -o Build/combinatorial_parser.o



Debug/main.o: Source/main.cpp Source/template_utility.hpp Source/combinatorial_parser.hpp Source/coro_base.hpp Source/lifted_state_machine.hpp Source/operator_tree.hpp Source/indirect_value.hpp Source/expression.hpp Source/expression_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/main.cpp -o Debug/main.o

Debug/expression.o: Source/expression.cpp Source/template_utility.hpp Source/expression.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/expression.cpp -o Debug/expression.o

Debug/expression_parser.o: Source/expression_parser.cpp Source/template_utility.hpp Source/combinatorial_parser.hpp Source/operator_tree_parser.hpp Source/operator_tree.hpp Source/expression_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/expression_parser.cpp -o Debug/expression_parser.o

Debug/combinatorial_parser.o: Source/combinatorial_parser.cpp Source/template_utility.hpp Source/combinatorial_parser.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/combinatorial_parser.cpp -o Debug/combinatorial_parser.o


.PHONY: clean
clean:
	-rm -r Build
	-rm -r Debug

.PHONY: regenerate
regenerate:
	python3 Tools/makefile_generator.py