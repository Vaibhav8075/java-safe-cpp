#pragma once

#include "Instruction.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <cstdint>

namespace jscpp {

struct FunctionMeta {
    std::string name;
    int entryPoint;
    int numLocals;
    int numArgs;
};

class BytecodeProgram {
public:
    std::vector<Instruction> instructions;
    std::vector<int> constantTable;
    std::unordered_map<std::string, FunctionMeta> functions;
    int mainEntryPoint = -1;

    int addConstant(int value);
    void addInstruction(const Instruction& inst);

    void dump(std::ostream& os) const;

    // Serialization
    void save(const std::string& filename) const;
    static BytecodeProgram load(const std::string& filename);
};

} // namespace jscpp
