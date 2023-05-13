# eventually add back -fsanitize=undefined; right now it doesn't seem to work
# on nixos
CFLAGS=-c -g --std=c++17
SOURCES=repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp vm.cpp stacktrace.cpp
OBJECTS=$(SOURCES:.cpp=.o)
# Gcc/Clang will create these .d files containing dependencies.
DEP=$(OBJECTS:%.o=%.d)

all: repl

repl: $(OBJECTS)
	g++ $(OBJECTS) -o gel -ldl

-include $(DEP)

# $@ is the name of the target being generated, and $< the first prerequisite
%.o : %.cpp
	g++ $(CFLAGS) -MMD $< -o $@

clean:
	-rm gel
	-rm *.o
	-rm *.d
