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

Build/program: Build/main.o Build/CombinatorComputation/combinator_interface.o Build/CombinatorComputation/primitives.o Build/CombinatorComputation/matching.o Build/TypeTheory/type_theory.o Build/WebInterface/web_interface.o Build/WebInterface/session.o 
	$(compiler_cmd)  Build/main.o Build/CombinatorComputation/combinator_interface.o Build/CombinatorComputation/primitives.o Build/CombinatorComputation/matching.o Build/TypeTheory/type_theory.o Build/WebInterface/web_interface.o Build/WebInterface/session.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O3 -o Build/program

Debug/program: Debug/main.o Debug/CombinatorComputation/combinator_interface.o Debug/CombinatorComputation/primitives.o Debug/CombinatorComputation/matching.o Debug/TypeTheory/type_theory.o Debug/WebInterface/web_interface.o Debug/WebInterface/session.o 
	$(compiler_cmd)  Debug/main.o Debug/CombinatorComputation/combinator_interface.o Debug/CombinatorComputation/primitives.o Debug/CombinatorComputation/matching.o Debug/TypeTheory/type_theory.o Debug/WebInterface/web_interface.o Debug/WebInterface/session.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -lpthread -O0 -ggdb -o Debug/program

Build/test_program: Build/CombinatorComputation/combinator_interface.o Build/CombinatorComputation/primitives.o Build/CombinatorComputation/matching.o Build/TypeTheory/type_theory.o Build/Tests/combinator_matching.o Build/Tests/test_main.o Build/Tests/parser_test.o 
	$(compiler_cmd)  Build/CombinatorComputation/combinator_interface.o Build/CombinatorComputation/primitives.o Build/CombinatorComputation/matching.o Build/TypeTheory/type_theory.o Build/Tests/combinator_matching.o Build/Tests/test_main.o Build/Tests/parser_test.o  -LDependencies/lib -O3 -o Build/test_program

Debug/test_program: Debug/CombinatorComputation/combinator_interface.o Debug/CombinatorComputation/primitives.o Debug/CombinatorComputation/matching.o Debug/TypeTheory/type_theory.o Debug/Tests/combinator_matching.o Debug/Tests/test_main.o Debug/Tests/parser_test.o 
	$(compiler_cmd)  Debug/CombinatorComputation/combinator_interface.o Debug/CombinatorComputation/primitives.o Debug/CombinatorComputation/matching.o Debug/TypeTheory/type_theory.o Debug/Tests/combinator_matching.o Debug/Tests/test_main.o Debug/Tests/parser_test.o  -LDependencies/lib -O0 -ggdb -o Debug/test_program

Source/Parser/parser_tree.hpp: Source/Parser/parser_tree.py
	python3 Tools/source_generator.py Source/Parser/parser_tree.py


Build/main.o: Source/main.cpp Source/Parser/parser_tree.hpp Source/Utility/indirect.hpp Source/WebInterface/web_interface.hpp Source/Utility/overloaded.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/main.cpp -o Build/main.o

Build/CombinatorComputation/combinator_interface.o: Source/CombinatorComputation/combinator_interface.cpp Source/CombinatorComputation/combinator_interface.hpp Source/CombinatorComputation/primitives.hpp Source/Format/tree_format.hpp Source/Utility/indirect.hpp Source/Utility/labeled_binary_tree.hpp Source/Utility/function.hpp Source/CombinatorComputation/matching.hpp Source/Utility/binary_tree.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/CombinatorComputation/combinator_interface.cpp -o Build/CombinatorComputation/combinator_interface.o

Build/CombinatorComputation/primitives.o: Source/CombinatorComputation/primitives.cpp Source/Utility/binary_tree.hpp Source/Utility/indirect.hpp Source/CombinatorComputation/primitives.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/CombinatorComputation/primitives.cpp -o Build/CombinatorComputation/primitives.o

Build/CombinatorComputation/matching.o: Source/CombinatorComputation/matching.cpp Source/Utility/binary_tree.hpp Source/CombinatorComputation/primitives.hpp Source/Utility/indirect.hpp Source/CombinatorComputation/matching.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/CombinatorComputation/matching.cpp -o Build/CombinatorComputation/matching.o

Build/TypeTheory/type_theory.o: Source/TypeTheory/type_theory.cpp Source/Utility/shared_variant.hpp Source/TypeTheory/type_theory.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/TypeTheory/type_theory.cpp -o Build/TypeTheory/type_theory.o

Build/WebInterface/web_interface.o: Source/WebInterface/web_interface.cpp Source/CombinatorComputation/combinator_interface.hpp Source/CombinatorComputation/primitives.hpp Source/Format/tree_format.hpp Source/Utility/indirect.hpp Source/Utility/labeled_binary_tree.hpp Source/Utility/function.hpp Source/WebInterface/session.hpp Source/Utility/binary_tree.hpp Source/Utility/overloaded.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/WebInterface/web_interface.cpp -o Build/WebInterface/web_interface.o

Build/WebInterface/session.o: Source/WebInterface/session.cpp Source/CombinatorComputation/combinator_interface.hpp Source/CombinatorComputation/primitives.hpp Source/Format/tree_format.hpp Source/Utility/indirect.hpp Source/Utility/labeled_binary_tree.hpp Source/Utility/function.hpp Source/WebInterface/session.hpp Source/Utility/binary_tree.hpp Source/Utility/overloaded.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/WebInterface/session.cpp -o Build/WebInterface/session.o

Build/Tests/combinator_matching.o: Source/Tests/combinator_matching.cpp Source/Utility/binary_tree.hpp Source/CombinatorComputation/primitives.hpp Source/Utility/indirect.hpp Source/CombinatorComputation/matching.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/combinator_matching.cpp -o Build/Tests/combinator_matching.o

Build/Tests/test_main.o: Source/Tests/test_main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/test_main.cpp -o Build/Tests/test_main.o

Build/Tests/parser_test.o: Source/Tests/parser_test.cpp Source/Utility/shared_variant.hpp Source/Parser/expression_parser.hpp Source/Parser/recursive_macros.hpp Source/Parser/parser.hpp Source/Parser/interface.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/Tests/parser_test.cpp -o Build/Tests/parser_test.o



Debug/main.o: Source/main.cpp Source/Parser/parser_tree.hpp Source/Utility/indirect.hpp Source/WebInterface/web_interface.hpp Source/Utility/overloaded.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/main.cpp -o Debug/main.o

Debug/CombinatorComputation/combinator_interface.o: Source/CombinatorComputation/combinator_interface.cpp Source/CombinatorComputation/combinator_interface.hpp Source/CombinatorComputation/primitives.hpp Source/Format/tree_format.hpp Source/Utility/indirect.hpp Source/Utility/labeled_binary_tree.hpp Source/Utility/function.hpp Source/CombinatorComputation/matching.hpp Source/Utility/binary_tree.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/CombinatorComputation/combinator_interface.cpp -o Debug/CombinatorComputation/combinator_interface.o

Debug/CombinatorComputation/primitives.o: Source/CombinatorComputation/primitives.cpp Source/Utility/binary_tree.hpp Source/Utility/indirect.hpp Source/CombinatorComputation/primitives.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/CombinatorComputation/primitives.cpp -o Debug/CombinatorComputation/primitives.o

Debug/CombinatorComputation/matching.o: Source/CombinatorComputation/matching.cpp Source/Utility/binary_tree.hpp Source/CombinatorComputation/primitives.hpp Source/Utility/indirect.hpp Source/CombinatorComputation/matching.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/CombinatorComputation/matching.cpp -o Debug/CombinatorComputation/matching.o

Debug/TypeTheory/type_theory.o: Source/TypeTheory/type_theory.cpp Source/Utility/shared_variant.hpp Source/TypeTheory/type_theory.hpp Source/Utility/overloaded.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/TypeTheory/type_theory.cpp -o Debug/TypeTheory/type_theory.o

Debug/WebInterface/web_interface.o: Source/WebInterface/web_interface.cpp Source/CombinatorComputation/combinator_interface.hpp Source/CombinatorComputation/primitives.hpp Source/Format/tree_format.hpp Source/Utility/indirect.hpp Source/Utility/labeled_binary_tree.hpp Source/Utility/function.hpp Source/WebInterface/session.hpp Source/Utility/binary_tree.hpp Source/Utility/overloaded.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/WebInterface/web_interface.cpp -o Debug/WebInterface/web_interface.o

Debug/WebInterface/session.o: Source/WebInterface/session.cpp Source/CombinatorComputation/combinator_interface.hpp Source/CombinatorComputation/primitives.hpp Source/Format/tree_format.hpp Source/Utility/indirect.hpp Source/Utility/labeled_binary_tree.hpp Source/Utility/function.hpp Source/WebInterface/session.hpp Source/Utility/binary_tree.hpp Source/Utility/overloaded.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/WebInterface/session.cpp -o Debug/WebInterface/session.o

Debug/Tests/combinator_matching.o: Source/Tests/combinator_matching.cpp Source/Utility/binary_tree.hpp Source/CombinatorComputation/primitives.hpp Source/Utility/indirect.hpp Source/CombinatorComputation/matching.hpp Source/Utility/tags.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/combinator_matching.cpp -o Debug/Tests/combinator_matching.o

Debug/Tests/test_main.o: Source/Tests/test_main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/test_main.cpp -o Debug/Tests/test_main.o

Debug/Tests/parser_test.o: Source/Tests/parser_test.cpp Source/Utility/shared_variant.hpp Source/Parser/expression_parser.hpp Source/Parser/recursive_macros.hpp Source/Parser/parser.hpp Source/Parser/interface.hpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/Tests/parser_test.cpp -o Debug/Tests/parser_test.o


.PHONY: clean
clean:
	-rm -r Build
	-rm -r Debug

.PHONY: regenerate
regenerate:
	python3 Tools/makefile_generator.py