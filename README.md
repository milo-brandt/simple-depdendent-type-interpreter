# Dependent Type Interpreter

This project implements a simple dependently typed language and an interpreter capable of simple type deduction, along with facilities for binding C++ types to the language.

A live demo of the project is hosted [here](https://themilobrandt.com/typetheory/), along with some basic information about its syntax.

# Compilation

At the moment, this project is mostly intended for my own interest - so I've yet to do nice things with deploying it. It should be possible to compile this code on any system with python3 (and the jinja2 python library), make, and a recent C++ compiler, though it's not been tested aside from on Ubuntu. It can compile both natively and for WebAssembly. It uses a significant amount of code generation, so cannot be compiled just by pointing a C++ compiler at the repository and hoping for the best.

If you have the prerequisites, you can generate a Makefile with the following command, while in the main directory:

```
python3 Tools/makefile_generator.py
```


Then `make debug` will compile the program and launch in gdb (or `make Debug/program` to just make the executable). It assumes it can use `g++-10` as a C++ compiler, though you can change the first line of the Makefile to use some other compiler. A target `make web_run` is also provided, which compiles the project with em++ and then runs a server on localhost:8000 and opens Firefox to that page (or `make WebBuild` to just compile with Emscripten). There are also targets `test`, `run`, `debug_test` and `clean`.

# Overview of Source

A good place to get a feel for the project is to look at the `full_compile` function defined in Source/Expression/interactive_environment.cpp and the functions it calls - this function integrates all the pieces together and lays out the compilation pipeline somewhat explicitly. In broad strokes, compilation has the following phases:

* Lex: Converts the string into a tree of tokens, ensuring that parenthesis, curly braces, and square brackets are matched.
* Parse: Consumes the lexed tree and organizes the tokens into a syntax tree.
* Resolution: Matches identifiers in the syntax tree with the global or local variables they refer to.
* Instruction Generation: Converts the syntax tree into a more linear series of requests to make against an interpreter context.
* Evaluation: Runs the instructions and produces a representation of the final expression along with a list of indeterminates and type equations to solve and rules to create.
* Solve: Loops through the equations from the previous step, creating definitions for indeterminates as is possible. Also verifies the correctness of rules and adds them to the interpreter as they pass.

Most of these stages - particularly the earlier ones - produce their output as trees. In addition to the output, stages also typically produce a tree with the same shape which gives information about the source of elements of the tree (e.g. where in the string a token appears, for the lexer). The files produced by the source code generator implement the utilities to easily produce such pairs of trees with the same shape and lookup information from either, using a shared system of indices. This allows the compiler to trace back through the outputs to produce useful error messages without requiring later stages to handle the locator trees. Some of the stages also produce trees allowing the compiler to trace forwards - though this is still a work in progress.

The Solve stage is the most complex. A Solver object handles all requests to check desired equalities and deduce the values of any holes in the program. It exposes a promise-based interface for this, allowing users of the object to request work be done and receive a callback when the work has been handled (or has spawned an error). The solver operates by applying the following rules to equations:

* Definition Extraction: An equation of the form `var $0 $1 $2 = RHS` where `var` is indeterminate and `RHS` does not depend nor on any arguments not on the LHS on it will be converted to a rule.
* Deepening: An equation of the form `F = G` where either side would simplify if more arguments were applied will spawn a new equation `F $x = G $x` where `$x` is an additional argument.
* Symmetric Explosion: An equation of the form `pair a b = pair x y` where both sides have no possibility of further simplification (even by deepening) will spawn new equations `a = x` and `b = y`. If a similar case happens where the equation heads differ, the equation will be marked false.
* Asymmetric Explosion: An equation of the form `var $0 $1 $2 = pair x y` where `var` is indeterminate and the RHS has no further simplifications allows us to redefine `var $0 $1 $2 = pair (var1 $0 $1 $2) (var2 $0 $1 $2)` for new indeterminates `var1` and `var2`. This rule is only used where definition extraction cannot be.
* Judgemental Equality: An equation where both sides are exactly equal may be marked as true.

The compiler mainly uses two higher level operations built on top of these:

* Casting: Where the evaluator encounters an expression which must have a particular type, but where it cannot be sure that it actually does, it produces a *cast*, which consists of an equation expressing equality of the observed and desired types, along with the introduction of a new indeterminate of the desired type - which is used in place of the actual expression. Once the equality of types is found to be true, the indeterminate is defined to be equal to the observed value and the interpreter may then eliminate the indeterminate in a way transparent to the user. There is a slight variant used when casting `f` in an application `f x` to cast `f` before any work is done on `x`, to prevent some very perplexing error messages that once existed.
* Rules: When the evaluator requests a rule, the pattern will first be type checked as an expression. Once that it complete, the compiler will generate a pattern consisting of *capture points* wherever the pattern includes an argument or an expression that cannot be matched against (i.e. anything other than an axiom). It will also list any relations that the pattern asserts between capture points. The pattern will then be type checked *again* with indeterminates placed at each capture point - and only allowing the solver to solve for these. Finally, any relations between capture points will be checked. Once this is complete, the rule is sent to the interpreter.

# Future Plans

At the moment, the implementation is very rough. Major improvements would include adding a trait system to allow easier programming, adding a module system, and making optimizations in the interpreter (which currently computes more than it needs to and doesn't simplify rules - and could probably benefit from memoization). Further in the future, it would also be nice to implement a more sensible formal system - likely cubical type theory - rather than the ad-hoc and slightly odd mathematical system currently implemented.
