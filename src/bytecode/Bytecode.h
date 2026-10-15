#pragma once

#include "Instruction.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

namespace jscpp {

struct FunctionMeta {
    std::string name;
    int entryPoint = 0;
    int numLocals = 0;
    int numArgs = 0;
};

class BytecodeProgram {
public:
    static constexpr int32_t FORMAT_VERSION = 2;

    std::vector<Instruction> instructions;
    std::vector<int> constantTable;
    std::vector<FunctionMeta> functions; // CALL's operand is an index into this table
    int mainEntryPoint = -1;

    int addConstant(int value);
    void addInstruction(const Instruction& inst);

    // Index of the named function, or -1
    int findFunction(const std::string& name) const;
    // Index of the function whose code contains the instruction at ip, or -1
    int functionAt(int ip) const;

    void dump(std::ostream& os) const;

    // Throws std::runtime_error if any operand points outside its table, so the VM never
    // has to trust a hand-edited or corrupted .jbc file
    void verify() const;

    // Serialization (little-endian on every host, so .jbc files are portable)
    void save(const std::string& filename) const;
    static BytecodeProgram load(const std::string& filename);
};

} // namespace jscpp
