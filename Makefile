all: repl

repl: repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp vm.cpp stacktrace.cpp
	g++ -g repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp vm.cpp stacktrace.cpp -o gel -fsanitize=undefined

clean:
	rm gel
