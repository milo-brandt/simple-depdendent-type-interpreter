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

Build/program: Build/main.o 
	$(compiler_cmd)  Build/main.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -O3 -o Build/program

Debug/program: Debug/main.o 
	$(compiler_cmd)  Debug/main.o  -LDependencies/lib -lhttpserver -Wl,-rpath='$$ORIGIN' -O0 -ggdb -o Debug/program

Build/test_program: 
	$(compiler_cmd)   -LDependencies/lib -O3 -o Build/test_program

Debug/test_program: 
	$(compiler_cmd)   -LDependencies/lib -O0 -ggdb -o Debug/test_program



Build/main.o: Source/main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O3 -IDependencies/include -c Source/main.cpp -o Build/main.o



Debug/main.o: Source/main.cpp 
	@mkdir -p $(@D)
	$(compiler_cmd) -O0 -ggdb -IDependencies/include -c Source/main.cpp -o Debug/main.o


.PHONY: clean
clean:
	-rm -r Build
	-rm -r Debug

.PHONY: regenerate
regenerate:
	python3 Tools/makefile_generator.py