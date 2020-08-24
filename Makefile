compiler = gcc-10
compiler_cmd = $(compiler) -std=c++20 -fcoroutines -ggdb3 -fconcepts-diagnostics-depth=10 -Wreturn-type -isystem /usr/local/include

src = $(shell find . -name "*.cpp")
obj = $(src:.cpp=.o)

all: program

program: $(obj)
	$(compiler_cmd)  $(obj) -lstdc++ -o program

%.o : %.cpp
		$(compiler_cmd) -c $< -o $@

.PHONY: clean
clean:
	rm -r *.o
