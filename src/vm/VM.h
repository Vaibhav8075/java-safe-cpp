#pragma once

#include "bytecode/Bytecode.h"
#include "RuntimeValue.h"
#include "Heap.h"
#include <vector>

namespace jscpp {

struct Frame {
    int returnAddress;
    int localBase;
    int numLocals;
};

class VM {
public:
    VM(const BytecodeProgram& program, bool trace = false);

    // Runs main. Returns 0 on success, or 1 if the program stopped with a runtime exception.
    int run();

    // Prints allocation / free counts; call after run()
    void printStats(std::ostream& os) const;

private:
    BytecodeProgram prog;
    bool trace;

    std::vector<RuntimeValue> stack;
    std::vector<Frame> callStack;
    std::vector<RuntimeValue> locals; // Combined locals for all frames

    int ip = 0;
    Heap heap;
    size_t liveAtExit = 0;

    // Config
    static const size_t MAX_CALL_STACK = 1000;
    static const size_t MAX_OPERAND_STACK = 100000;

    // Helpers
    void push(RuntimeValue v);
    RuntimeValue pop();
    RuntimeValue& peek(size_t depth);
    RuntimeValue& local(int index);

    // Dispatch
    void executeInstruction(const Instruction& inst);
    void traceInstruction(const Instruction& inst) const;
};

} // namespace jscpp
