#include "Bytecode.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace jscpp {

int BytecodeProgram::addConstant(int value) {
    for (size_t i = 0; i < constantTable.size(); ++i) {
        if (constantTable[i] == value) return i;
    }
    constantTable.push_back(value);
    return constantTable.size() - 1;
}

void BytecodeProgram::addInstruction(const Instruction& inst) {
    instructions.push_back(inst);
}

void BytecodeProgram::dump(std::ostream& os) const {
    os << "=== CONSTANT TABLE ===\n";
    for (size_t i = 0; i < constantTable.size(); ++i) {
        os << "[" << i << "] = " << constantTable[i] << "\n";
    }

    os << "\n=== FUNCTIONS ===\n";
    for (const auto& [name, meta] : functions) {
        os << name << ": entry=" << meta.entryPoint
           << " locals=" << meta.numLocals
           << " args=" << meta.numArgs << "\n";
    }

    os << "\n=== CODE ===\n";
    for (size_t i = 0; i < instructions.size(); ++i) {
        os << std::setw(4) << std::setfill('0') << i << "  "
           << instructions[i].toString() << "\n";
    }
}

// Format:
// MAGIC (4 bytes): 'J' 'S' 'C' 'P'
// VERSION (4 bytes): 1
// MAIN_ENTRY (4 bytes)
// CONSTANT_TABLE_SIZE (4 bytes)
// CONSTANT_TABLE (4 bytes each)
// FUNCTIONS_SIZE (4 bytes)
// FUNCTIONS (name len, name, entry, numLocals, numArgs)
// CODE_SIZE (4 bytes)
// CODE (opcode 4 bytes, operand 4 bytes)

void BytecodeProgram::save(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open file for writing: " + filename);

    auto write32 = [&out](int32_t val) { out.write(reinterpret_cast<const char*>(&val), 4); };

    out.write("JSCP", 4);
    write32(1);
    write32(mainEntryPoint);

    write32(constantTable.size());
    for (int val : constantTable) write32(val);

    write32(functions.size());
    for (const auto& [name, meta] : functions) {
        write32(name.size());
        out.write(name.data(), name.size());
        write32(meta.entryPoint);
        write32(meta.numLocals);
        write32(meta.numArgs);
    }

    write32(instructions.size());
    for (const auto& inst : instructions) {
        write32(static_cast<int32_t>(inst.op));
        write32(inst.operand);
    }
}

BytecodeProgram BytecodeProgram::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file for reading: " + filename);

    auto read32 = [&in]() {
        int32_t val;
        in.read(reinterpret_cast<char*>(&val), 4);
        return val;
    };

    char magic[4];
    in.read(magic, 4);
    if (std::string(magic, 4) != "JSCP") throw std::runtime_error("Invalid magic number");

    if (read32() != 1) throw std::runtime_error("Unsupported version");

    BytecodeProgram prog;
    prog.mainEntryPoint = read32();

    int constSize = read32();
    for (int i = 0; i < constSize; ++i) prog.constantTable.push_back(read32());

    int funcSize = read32();
    for (int i = 0; i < funcSize; ++i) {
        int nameLen = read32();
        std::string name(nameLen, '\0');
        in.read(&name[0], nameLen);
        FunctionMeta meta;
        meta.name = name;
        meta.entryPoint = read32();
        meta.numLocals = read32();
        meta.numArgs = read32();
        prog.functions[name] = meta;
    }

    int instSize = read32();
    for (int i = 0; i < instSize; ++i) {
        Opcode op = static_cast<Opcode>(read32());
        int operand = read32();
        prog.instructions.emplace_back(op, operand);
    }

    return prog;
}

} // namespace jscpp
