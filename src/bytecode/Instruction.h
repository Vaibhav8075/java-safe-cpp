#pragma once

#include "Opcode.h"
#include <string>

namespace jscpp {

struct Instruction {
    Opcode op;
    int operand; // Integer operand (e.g. index in const table, local offset, jump target)
    int line;    // Source line that produced this instruction, used in runtime error messages

    Instruction(Opcode op, int operand = 0, int line = 0) : op(op), operand(operand), line(line) {}

    std::string toString() const {
        std::string res = opcodeToString(op);
        // Only specific opcodes take operands
        if (op == Opcode::PUSH_CONST || op == Opcode::LOAD_LOCAL ||
            op == Opcode::STORE_LOCAL || op == Opcode::JUMP ||
            op == Opcode::JUMP_IF_FALSE || op == Opcode::CALL ||
            op == Opcode::ALLOC) {
            res += " " + std::to_string(operand);
        }
        return res;
    }
};

} // namespace jscpp
