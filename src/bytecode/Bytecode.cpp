#include "Bytecode.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace jscpp {

int BytecodeProgram::addConstant(int value) {
    for (size_t i = 0; i < constantTable.size(); ++i) {
        if (constantTable[i] == value) return static_cast<int>(i);
    }
    constantTable.push_back(value);
    return static_cast<int>(constantTable.size()) - 1;
}

void BytecodeProgram::addInstruction(const Instruction& inst) {
    instructions.push_back(inst);
}

int BytecodeProgram::findFunction(const std::string& name) const {
    for (size_t i = 0; i < functions.size(); ++i) {
        if (functions[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

int BytecodeProgram::functionAt(int ip) const {
    int best = -1;
    for (size_t i = 0; i < functions.size(); ++i) {
        if (functions[i].entryPoint <= ip && (best == -1 || functions[i].entryPoint > functions[best].entryPoint)) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

void BytecodeProgram::dump(std::ostream& os) const {
    os << "=== CONSTANT TABLE ===\n";
    for (size_t i = 0; i < constantTable.size(); ++i) {
        os << "[" << i << "] = " << constantTable[i] << "\n";
    }

    os << "\n=== FUNCTIONS ===\n";
    for (size_t i = 0; i < functions.size(); ++i) {
        const auto& meta = functions[i];
        os << "#" << i << " " << meta.name << ": entry=" << meta.entryPoint
           << " locals=" << meta.numLocals
           << " args=" << meta.numArgs << "\n";
    }

    os << "\n=== CODE ===\n";
    for (size_t i = 0; i < instructions.size(); ++i) {
        for (const auto& meta : functions) {
            if (meta.entryPoint == static_cast<int>(i)) os << meta.name << ":\n";
        }
        std::string text = instructions[i].toString();
        if (instructions[i].op == Opcode::CALL && instructions[i].operand >= 0 &&
            instructions[i].operand < static_cast<int>(functions.size())) {
            text += "  (" + functions[instructions[i].operand].name + ")";
        }
        if (instructions[i].op == Opcode::PUSH_CONST && instructions[i].operand >= 0 &&
            instructions[i].operand < static_cast<int>(constantTable.size())) {
            text += "  (=" + std::to_string(constantTable[instructions[i].operand]) + ")";
        }
        os << "  " << std::setw(4) << std::setfill('0') << i << "  " << std::left << std::setw(28)
           << std::setfill(' ') << text << std::right << "; line " << instructions[i].line << "\n";
    }
}

void BytecodeProgram::verify() const {
    const int codeSize = static_cast<int>(instructions.size());
    const int numConsts = static_cast<int>(constantTable.size());
    const int numFuncs = static_cast<int>(functions.size());

    bool mainFound = false;
    for (const auto& meta : functions) {
        if (meta.entryPoint < 0 || meta.entryPoint >= codeSize) {
            throw std::runtime_error("Bytecode verify: function " + meta.name + " has an invalid entry point.");
        }
        if (meta.numArgs < 0 || meta.numLocals < meta.numArgs || meta.numLocals > 65536) {
            throw std::runtime_error("Bytecode verify: function " + meta.name + " has invalid local/arg counts.");
        }
        if (meta.name == "main" && meta.entryPoint == mainEntryPoint) mainFound = true;
    }
    if (!mainFound) {
        throw std::runtime_error("Bytecode verify: no main function.");
    }

    for (int i = 0; i < codeSize; ++i) {
        const Instruction& inst = instructions[i];
        int op = static_cast<int>(inst.op);
        if (op < 0 || op > static_cast<int>(Opcode::HALT)) {
            throw std::runtime_error("Bytecode verify: unknown opcode " + std::to_string(op) + " at " + std::to_string(i));
        }
        bool ok = true;
        switch (inst.op) {
            case Opcode::PUSH_CONST: ok = inst.operand >= 0 && inst.operand < numConsts; break;
            case Opcode::LOAD_LOCAL:
            case Opcode::STORE_LOCAL: ok = inst.operand >= 0 && inst.operand < 65536; break;
            case Opcode::JUMP:
            case Opcode::JUMP_IF_FALSE: ok = inst.operand >= 0 && inst.operand <= codeSize; break;
            case Opcode::CALL: ok = inst.operand >= 0 && inst.operand < numFuncs; break;
            case Opcode::ALLOC: ok = inst.operand > 0 && inst.operand <= (1 << 20); break;
            default: break;
        }
        if (!ok) {
            throw std::runtime_error("Bytecode verify: bad operand " + std::to_string(inst.operand) + " for " +
                                     opcodeToString(inst.op) + " at " + std::to_string(i));
        }
    }
}

// File format (version 2), every integer is 4 bytes little-endian:
//   MAGIC 'J' 'S' 'C' 'P' | VERSION | MAIN_ENTRY
//   CONSTANT_COUNT | constants...
//   FUNCTION_COUNT | (name length, name bytes, entry, numLocals, numArgs)...
//   CODE_COUNT     | (opcode, operand, source line)...

static void writeLE(std::ostream& out, int32_t value) {
    uint32_t v = static_cast<uint32_t>(value);
    char bytes[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                     static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
    out.write(bytes, 4);
}

static int32_t readLE(std::istream& in) {
    unsigned char bytes[4];
    if (!in.read(reinterpret_cast<char*>(bytes), 4)) {
        throw std::runtime_error("Bytecode file is truncated.");
    }
    uint32_t v = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
    return static_cast<int32_t>(v);
}

static int32_t readCount(std::istream& in, int32_t limit, const char* what) {
    int32_t n = readLE(in);
    if (n < 0 || n > limit) {
        throw std::runtime_error(std::string("Bytecode file has an invalid ") + what + " count.");
    }
    return n;
}

void BytecodeProgram::save(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open file for writing: " + filename);

    out.write("JSCP", 4);
    writeLE(out, FORMAT_VERSION);
    writeLE(out, mainEntryPoint);

    writeLE(out, static_cast<int32_t>(constantTable.size()));
    for (int val : constantTable) writeLE(out, val);

    writeLE(out, static_cast<int32_t>(functions.size()));
    for (const auto& meta : functions) {
        writeLE(out, static_cast<int32_t>(meta.name.size()));
        out.write(meta.name.data(), static_cast<std::streamsize>(meta.name.size()));
        writeLE(out, meta.entryPoint);
        writeLE(out, meta.numLocals);
        writeLE(out, meta.numArgs);
    }

    writeLE(out, static_cast<int32_t>(instructions.size()));
    for (const auto& inst : instructions) {
        writeLE(out, static_cast<int32_t>(inst.op));
        writeLE(out, inst.operand);
        writeLE(out, inst.line);
    }
    if (!out) throw std::runtime_error("Failed while writing: " + filename);
}

BytecodeProgram BytecodeProgram::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file for reading: " + filename);

    char magic[4];
    if (!in.read(magic, 4) || std::string(magic, 4) != "JSCP") {
        throw std::runtime_error("Not a jscpp bytecode file (bad magic number).");
    }
    int32_t version = readLE(in);
    if (version != FORMAT_VERSION) {
        throw std::runtime_error("Unsupported bytecode version " + std::to_string(version) +
                                 " (expected " + std::to_string(FORMAT_VERSION) + "); recompile the source.");
    }

    BytecodeProgram prog;
    prog.mainEntryPoint = readLE(in);

    int32_t constSize = readCount(in, 1 << 20, "constant");
    for (int i = 0; i < constSize; ++i) prog.constantTable.push_back(readLE(in));

    int32_t funcSize = readCount(in, 1 << 16, "function");
    for (int i = 0; i < funcSize; ++i) {
        int32_t nameLen = readCount(in, 1024, "name length");
        std::string name(nameLen, '\0');
        if (nameLen > 0 && !in.read(&name[0], nameLen)) throw std::runtime_error("Bytecode file is truncated.");
        FunctionMeta meta;
        meta.name = name;
        meta.entryPoint = readLE(in);
        meta.numLocals = readLE(in);
        meta.numArgs = readLE(in);
        prog.functions.push_back(meta);
    }

    int32_t instSize = readCount(in, 1 << 24, "instruction");
    for (int i = 0; i < instSize; ++i) {
        Opcode op = static_cast<Opcode>(readLE(in));
        int operand = readLE(in);
        int line = readLE(in);
        prog.instructions.emplace_back(op, operand, line);
    }

    prog.verify();
    return prog;
}

} // namespace jscpp
