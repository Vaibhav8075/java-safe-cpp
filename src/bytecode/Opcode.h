#pragma once

#include <string>

namespace jscpp {

enum class Opcode {
    PUSH_CONST,
    LOAD_LOCAL,
    STORE_LOCAL,

    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, GT, LE, GE,
    AND, OR, NOT,

    JUMP,
    JUMP_IF_FALSE,

    CALL,
    RETURN,

    ALLOC,
    INC_REF,
    DEC_REF,

    LOAD_ARRAY,
    STORE_ARRAY,
    STORE_ARRAY_INV,
    CHECK_BOUNDS,

    CHECK_NULL,
    LOAD_POINTER,
    STORE_POINTER,

    PRINT,
    POP,
    HALT
};

inline std::string opcodeToString(Opcode op) {
    switch (op) {
        case Opcode::PUSH_CONST: return "PUSH_CONST";
        case Opcode::LOAD_LOCAL: return "LOAD_LOCAL";
        case Opcode::STORE_LOCAL: return "STORE_LOCAL";
        case Opcode::ADD: return "ADD";
        case Opcode::SUB: return "SUB";
        case Opcode::MUL: return "MUL";
        case Opcode::DIV: return "DIV";
        case Opcode::MOD: return "MOD";
        case Opcode::EQ: return "EQ";
        case Opcode::NE: return "NE";
        case Opcode::LT: return "LT";
        case Opcode::GT: return "GT";
        case Opcode::LE: return "LE";
        case Opcode::GE: return "GE";
        case Opcode::AND: return "AND";
        case Opcode::OR: return "OR";
        case Opcode::NOT: return "NOT";
        case Opcode::JUMP: return "JUMP";
        case Opcode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case Opcode::CALL: return "CALL";
        case Opcode::RETURN: return "RETURN";
        case Opcode::ALLOC: return "ALLOC";
        case Opcode::INC_REF: return "INC_REF";
        case Opcode::DEC_REF: return "DEC_REF";
        case Opcode::LOAD_ARRAY: return "LOAD_ARRAY";
        case Opcode::STORE_ARRAY: return "STORE_ARRAY";
        case Opcode::STORE_ARRAY_INV: return "STORE_ARRAY_INV";
        case Opcode::CHECK_BOUNDS: return "CHECK_BOUNDS";
        case Opcode::CHECK_NULL: return "CHECK_NULL";
        case Opcode::LOAD_POINTER: return "LOAD_POINTER";
        case Opcode::STORE_POINTER: return "STORE_POINTER";
        case Opcode::PRINT: return "PRINT";
        case Opcode::POP: return "POP";
        case Opcode::HALT: return "HALT";
        default: return "UNKNOWN";
    }
}

} // namespace jscpp
