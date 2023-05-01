#include "vm.h"

struct vm_error : public lisp_error { using lisp_error::lisp_error; };

bool is_bytecode(lref operand) {
    return std::dynamic_pointer_cast<Bytecode>(operand) != nullptr;
}

// TODO: might be some better way to do this but I don't feel like messing with the
// preprocessor
std::string Instruction::repr() const {
    return opcode_names[(int)code] + " "     \
            + (is_bytecode(operand) ? "<code>" : try_repr(operand));
}

Opcode sym_to_opcode(lref sym) {
    auto s = std::dynamic_pointer_cast<Symbol>(sym)->name;
    for (int i = 0; i < (int)Opcode::NUM_OPCODES; i++) {
        if (s == opcode_names[i]) {
            return (Opcode)i;
        }
    }

    throw assembler_error("Bad opcode name: " + s);
}

std::vector<Instruction> assemble(lref lst) {
    std::vector<Instruction> bytecode;
    while (lst != Nil) {
        if (len(car(lst)) == 1) {
            bytecode.push_back(Instruction(sym_to_opcode(car(car(lst))), Nil));
        } else if (len(car(lst)) == 2) {
            bytecode.push_back(Instruction(sym_to_opcode(car(car(lst))), cadr(car(lst))));
        } else {
            throw assembler_error("Bad number of arguments in opcode: " + try_repr(car(lst))
                                  + "; Expected 0 or 1");
        }
        lst = cdr(lst);
    }
    return bytecode;
}

std::string print_bytecode(const std::vector<Instruction>& bytecode) {
    std::string ret;
    for (int i = 0; i < bytecode.size(); i++) {
        ret += bytecode[i].repr() + "\n";
    }
    return ret;
}

struct Continuation : LispObject {
    std::shared_ptr<Bytecode> block;
    unsigned long pc;

    Continuation(const std::shared_ptr<Bytecode>& block, unsigned long pc) : block(block), pc(pc) {}
    std::string repr() const override { return "<Continuation>"; }
    std::string type_string() const override { return "continuation"; }
};

lref run_bytecode(const lref& block) {
    auto bytc = std::dynamic_pointer_cast<Bytecode>(block);
    if (bytc == nullptr) {
        throw vm_error("Trying to run something that isn't bytecode.");
    }

    lref stack[GEL_MAX_STACK_SIZE];
    int stack_size = 0;
    std::shared_ptr<Bytecode> current_block = bytc;

    for (std::vector<Instruction>::size_type pc = 0; pc < current_block->code.size(); pc++) {
        switch(current_block->code[pc].code) {
            case Opcode::PUSH:
                stack[stack_size++] = current_block->code[pc].operand;
                break;
            case Opcode::CONS:
            {
                if (stack_size < 2) {
                    throw vm_error("Not enough arguments to CONS.");
                }
                lref car = stack[--stack_size];
                lref cdr = stack[--stack_size];
                stack[stack_size++] = cons(car, cdr);
            }
                break;
            case Opcode::CALL_BUILTIN:
            {
                lref arglist = stack[--stack_size];
                auto lfn = std::dynamic_pointer_cast<LispFunction>(current_block->code[pc].operand);
                if (lfn == nullptr) {
                    throw vm_error("CALL_BUILTIN takes a function.");
                }

                stack[stack_size++] = lfn->value(arglist);
                break;
            }
            case Opcode::CALL:
                /* Problem: how do we get the program counter to point to the code we need
                 * to execute if said code is buried in some random object?
                 * We can't overwrite the bytecode arg because then we don't know where to
                 * go back to
                 * We could recursively call run_bytecode but then we lose stack info,
                 * making the debugger not work
                 * We could append the bytecode to the bytecode arg, but that seems hard to
                 * debug if it goes wrong, and creates a lot of overhead for each function
                 * call
                 * We could create some global store of bytecode, like an actual compiled
                 * C++ program, but then we need to do shenanigans to keep track of addresses
                 * and rewrite it whenever something is recompiled, which is complicated
                 *
                 * Solution: store a pointer to the block of code we're in in addition to the
                 * program counter
                 * All we really care about is that the code we need to execute exists
                 * _somewhere_ in memory, not _where_ it is
                 *
                 * We can push the old code block lref along with the old program counter
                 * so we know where to jump back to
                 */
            {
                auto new_block_lref = current_block->code[pc].operand;
                auto new_block = std::dynamic_pointer_cast<Bytecode>(new_block_lref);
                if (new_block == nullptr) {
                    throw vm_error("Tried to jump to something that isn't code. This is very bad.");
                }
                stack[stack_size++] = std::make_shared<Continuation>(current_block, pc);
                current_block = new_block;
                pc = -1;  // Just going to increment it
            }
                break;
            case Opcode::RET:
            {
                auto return_addr = std::dynamic_pointer_cast<Continuation>(stack[--stack_size]);
                if (return_addr == nullptr) {
                    throw vm_error("Not a continuation.");
                }

                auto new_block_lref = return_addr->block;
                auto new_block = std::dynamic_pointer_cast<Bytecode>(new_block_lref);
                if (new_block == nullptr) {
                    throw vm_error("Tried to jump (via RET) to null. This is very bad.");
                }

                current_block = return_addr->block;
                pc = return_addr->pc;
            }
                break;
            case Opcode::POP:
                // TODO: clean up the ref?
                stack_size--;
                break;
            case Opcode::JIF:
            {
                auto addr = std::dynamic_pointer_cast<LispInt>(current_block->code[pc].operand);
                if (addr == nullptr) {
                    throw vm_error("Jump address is null.");
                }
                if (addr < 0) {
                    throw vm_error("Jump address is < 0.");
                }
                auto arg1 = stack[--stack_size];
                if (arg1 != Nil && arg1 != False) {
                    // -1 because we're about to increment it
                    pc = (unsigned long)(addr->val) - 1;
                }
            }
                break;
            case Opcode::JMP:
            {
                auto addr = std::dynamic_pointer_cast<LispInt>(current_block->code[pc].operand);
                if (addr == nullptr) {
                    throw vm_error("Jump address is null.");
                }
                if (addr < 0) {
                    throw vm_error("Jump address is < 0.");
                }
                pc = (unsigned long)(addr->val) - 1;
            }
                break;
            default:
                throw vm_error("Unrecognized opcode.");
                break;
        }
    }

    // TODO: horrible and hacky
    return stack[0];
}
