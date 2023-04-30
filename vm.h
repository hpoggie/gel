#ifndef VM_H
#define VM_H

#include "types.h"

struct assembler_error : public lisp_error { using lisp_error::lisp_error; };

enum class Opcode {
PUSH,
CONS,
CALL_BUILTIN,
CALL,
RET,
POP,
JEQ,
NUM_OPCODES
};

const std::string opcode_names[(unsigned long)Opcode::NUM_OPCODES] = {
    "PUSH",
    "CONS",
    "CALL_BUILTIN",
    "CALL",
    "RET",
    "POP",
    "JEQ",
};

struct Instruction : LispObject {
    Opcode code;
    lref operand;

    Instruction(Opcode code, lref operand) : code(code), operand(operand) {}
    std::string repr() const;
};

const int GEL_MAX_STACK_SIZE = 1024;

std::vector<Instruction> assemble(lref lst);
std::string print_bytecode(const std::vector<Instruction>& bytecode);
lref run_bytecode(const lref& bytecode);

struct Bytecode : LispObject {
    std::vector<Instruction> code;

    Bytecode(std::vector<Instruction> code) : code(code) {}
    std::string repr () const { return print_bytecode(code); }
    std::string type_string() const { return "bytecode"; }
};

#endif
