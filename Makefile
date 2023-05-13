# eventually add back -fsanitize=undefined; right now it doesn't seem to work
# on nixos
CFLAGS=-c -g --std=c++17
SOURCES=repl.cpp types.cpp reader.cpp evaluator.cpp builtin.cpp vm.cpp stacktrace.cpp
OBJECTS=$(patsubst %.cpp, build/%.o, $(SOURCES))
# Gcc/Clang will create these .d files containing dependencies.
DEP=$(OBJECTS:%.o=%.d)

all: repl

repl: $(OBJECTS)
	g++ $(OBJECTS) -o gel -ldl

-include $(DEP)

# $@ is the name of the target being generated, and $< the first prerequisite
build/%.o : %.cpp build_dir
	g++ $(CFLAGS) -MMD $< -o $@

build_dir:
	mkdir -p build

clean:
	-rm -rf gel build
