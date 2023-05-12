# eventually add back -fsanitize=undefined; right now it doesn't seem to work
# on nixos
CFLAGS=-c -g --std=c++17
SOURCES=repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp vm.cpp stacktrace.cpp
OBJECTS=$(SOURCES:.cpp=.o)

all: repl

repl: $(OBJECTS)
	g++ $(OBJECTS) -o gel -ldl

# $@ is the name of the target being generated, and $< the first prerequisite
.cpp.o:
	g++ $(CFLAGS) $< -o $@

clean:
	rm gel
