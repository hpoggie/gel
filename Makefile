all: repl

repl: repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp
	g++ -g repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp stacktrace.cpp -o gel -fsanitize=undefined

clean:
	rm gel
