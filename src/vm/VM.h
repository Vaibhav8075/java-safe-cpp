#pragma once

#include "bytecode/Bytecode.h"
#include "RuntimeValue.h"
#include "Heap.h"
#include <vector>

namespace jscpp {

struct Frame {
    int returnAddress;
    int localBase;
};

class VM {
public:
    VM(const BytecodeProgram& program, bool trace = false);

    void run();

private:
    BytecodeProgram prog;
    bool trace;

    std::vector<RuntimeValue> stack;
    std::vector<Frame> callStack;
    std::vector<RuntimeValue> locals; // Combined locals for all frames

    int ip = 0;
    Heap heap;

    // Config
    static const int MAX_CALL_STACK = 1000;

    // Helpers
    void push(RuntimeValue v);
    RuntimeValue pop();

    // Dispatch
    void executeInstruction(const Instruction& inst);
};

} // namespace jscpp
