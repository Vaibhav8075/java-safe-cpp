#include "VM.h"
#include "RuntimeError.h"
#include <climits>
#include <cstdint>
#include <iostream>
#include <iomanip>

namespace jscpp {

// Signed overflow is undefined behaviour in C++, so arithmetic is done on uint32_t and
// wraps around exactly like Java's int (2147483647 + 1 == -2147483648).
static int wrap(uint32_t v) { return static_cast<int>(v); }
static int addInt(int a, int b) { return wrap(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
static int subInt(int a, int b) { return wrap(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
static int mulInt(int a, int b) { return wrap(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

VM::VM(const BytecodeProgram& program, bool trace)
    : prog(program), trace(trace) {
    prog.verify();
}

void VM::push(RuntimeValue v) {
    if (stack.size() >= MAX_OPERAND_STACK) {
        throw RuntimeError("Operand stack overflow.");
    }
    stack.push_back(v);
}

RuntimeValue VM::pop() {
    if (stack.empty()) {
        throw InvalidInstructionException("Operand stack underflow.");
    }
    RuntimeValue v = stack.back();
    stack.pop_back();
    return v;
}

RuntimeValue& VM::peek(size_t depth) {
    if (depth >= stack.size()) {
        throw InvalidInstructionException("Operand stack underflow.");
    }
    return stack[stack.size() - 1 - depth];
}

RuntimeValue& VM::local(int index) {
    const Frame& frame = callStack.back();
    if (index < 0 || index >= frame.numLocals) {
        throw InvalidInstructionException("Local variable slot " + std::to_string(index) + " out of range.");
    }
    return locals[frame.localBase + index];
}

int VM::run() {
    int mainIdx = prog.findFunction("main");
    const FunctionMeta& mainMeta = prog.functions[mainIdx];

    ip = mainMeta.entryPoint;
    callStack.push_back({-1, 0, mainMeta.numLocals});
    locals.assign(mainMeta.numLocals, RuntimeValue());

    int status = 0;
    try {
        while (ip >= 0 && ip < static_cast<int>(prog.instructions.size())) {
            const Instruction& inst = prog.instructions[ip++];
            if (trace) traceInstruction(inst);
            executeInstruction(inst);
            if (inst.op == Opcode::HALT) break;
        }
    } catch (const std::exception& e) {
        int at = ip - 1;
        int func = prog.functionAt(at);
        std::cout.flush();
        std::cerr << "Runtime Error:\n" << e.what() << "\n";
        if (at >= 0 && at < static_cast<int>(prog.instructions.size())) {
            std::cerr << "    at line " << prog.instructions[at].line;
            if (func >= 0) std::cerr << " in " << prog.functions[func].name << "()";
            std::cerr << " (instruction " << at << ")\n";
        }
        status = 1;
    }

    liveAtExit = heap.liveObjects();
    heap.cleanup();
    return status;
}

void VM::printStats(std::ostream& os) const {
    const HeapStats& s = heap.stats();
    os << "[heap] allocations=" << s.allocations << " frees=" << s.frees
       << " peak-live=" << s.peakLive << " live-at-exit=" << liveAtExit << "\n";
}

void VM::traceInstruction(const Instruction& inst) const {
    std::cout << "IP=" << std::setw(4) << std::setfill('0') << (ip - 1) << std::setfill(' ')
              << "  " << std::left << std::setw(20) << inst.toString() << std::right << " stack=[";
    size_t from = stack.size() > 8 ? stack.size() - 8 : 0;
    if (from > 0) std::cout << "... ";
    for (size_t i = from; i < stack.size(); ++i) {
        std::cout << stack[i].value << (i + 1 < stack.size() ? ", " : "");
    }
    std::cout << "]\n";
}

void VM::executeInstruction(const Instruction& inst) {
    switch (inst.op) {
        case Opcode::PUSH_CONST:
            push(RuntimeValue(prog.constantTable[inst.operand])); // index checked by verify()
            break;
        case Opcode::LOAD_LOCAL:
            push(local(inst.operand));
            break;
        case Opcode::STORE_LOCAL: {
            RuntimeValue v = pop();
            local(inst.operand) = v;
            break;
        }
        case Opcode::ADD: {
            int b = pop().value;
            int a = pop().value;
            push(addInt(a, b));
            break;
        }
        case Opcode::SUB: {
            int b = pop().value;
            int a = pop().value;
            push(subInt(a, b));
            break;
        }
        case Opcode::MUL: {
            int b = pop().value;
            int a = pop().value;
            push(mulInt(a, b));
            break;
        }
        case Opcode::DIV: {
            int b = pop().value;
            int a = pop().value;
            if (b == 0) throw DivisionByZeroException();
            push(a == INT_MIN && b == -1 ? INT_MIN : a / b); // INT_MIN / -1 overflows in C++
            break;
        }
        case Opcode::MOD: {
            int b = pop().value;
            int a = pop().value;
            if (b == 0) throw DivisionByZeroException();
            push(a == INT_MIN && b == -1 ? 0 : a % b);
            break;
        }
        case Opcode::EQ: {
            int b = pop().value;
            int a = pop().value;
            push(a == b);
            break;
        }
        case Opcode::NE: {
            int b = pop().value;
            int a = pop().value;
            push(a != b);
            break;
        }
        case Opcode::LT: {
            int b = pop().value;
            int a = pop().value;
            push(a < b);
            break;
        }
        case Opcode::GT: {
            int b = pop().value;
            int a = pop().value;
            push(a > b);
            break;
        }
        case Opcode::LE: {
            int b = pop().value;
            int a = pop().value;
            push(a <= b);
            break;
        }
        case Opcode::GE: {
            int b = pop().value;
            int a = pop().value;
            push(a >= b);
            break;
        }
        case Opcode::AND: {
            int b = pop().value;
            int a = pop().value;
            push(a && b);
            break;
        }
        case Opcode::OR: {
            int b = pop().value;
            int a = pop().value;
            push(a || b);
            break;
        }
        case Opcode::NOT: {
            int a = pop().value;
            push(!a);
            break;
        }
        case Opcode::JUMP:
            ip = inst.operand;
            break;
        case Opcode::JUMP_IF_FALSE: {
            int cond = pop().value;
            if (!cond) {
                ip = inst.operand;
            }
            break;
        }
        case Opcode::CALL: {
            const FunctionMeta& target = prog.functions[inst.operand]; // index checked by verify()

            if (callStack.size() >= MAX_CALL_STACK) {
                throw StackOverflowException();
            }

            // Args were pushed left to right, so the last argument is on top of the stack
            int newLocalBase = static_cast<int>(locals.size());
            locals.resize(locals.size() + target.numLocals);
            for (int i = target.numArgs - 1; i >= 0; --i) {
                locals[newLocalBase + i] = pop();
            }

            callStack.push_back({ip, newLocalBase, target.numLocals});
            ip = target.entryPoint;
            break;
        }
        case Opcode::RETURN: {
            RuntimeValue retVal = pop();

            Frame frame = callStack.back();
            callStack.pop_back();

            locals.resize(frame.localBase);
            ip = frame.returnAddress;

            if (callStack.empty()) {
                // Returned from main
                ip = static_cast<int>(prog.instructions.size());
            } else {
                push(retVal);
            }
            break;
        }
        case Opcode::ALLOC: {
            int ptr = heap.allocate(inst.operand);
            push(ptr);
            break;
        }
        case Opcode::INC_REF: {
            heap.incRef(peek(0).value);
            break;
        }
        case Opcode::DEC_REF: {
            int ptr = pop().value;
            heap.decRef(ptr);
            break;
        }
        case Opcode::LOAD_ARRAY: {
            // Stack: [ptr, index]. The VM re-checks bounds even without CHECK_BOUNDS,
            // so hand-written bytecode cannot read outside an object either
            int index = pop().value;
            int ptr = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            if (index < 0 || index >= obj.size) throw ArrayIndexOutOfBoundsException(index, obj.size);
            push(obj.data[index]);
            break;
        }
        case Opcode::STORE_ARRAY: {
            // Stack: [ptr, index, value]
            int val = pop().value;
            int index = pop().value;
            int ptr = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            if (index < 0 || index >= obj.size) throw ArrayIndexOutOfBoundsException(index, obj.size);
            obj.data[index] = val;
            break;
        }
        case Opcode::STORE_ARRAY_INV: {
            // Stack: [value, ptr, index]
            int index = pop().value;
            int ptr = pop().value;
            int val = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            if (index < 0 || index >= obj.size) throw ArrayIndexOutOfBoundsException(index, obj.size);
            obj.data[index] = val;
            push(val); // Assignment returns RHS value
            break;
        }
        case Opcode::CHECK_BOUNDS: {
            // Stack: [..., ptr, index] (does not pop)
            int index = peek(0).value;
            int ptr = peek(1).value;
            ManagedObject& obj = heap.getObject(ptr);
            if (index < 0 || index >= obj.size) {
                throw ArrayIndexOutOfBoundsException(index, obj.size);
            }
            break;
        }
        case Opcode::CHECK_NULL: {
            // Stack: [..., ptr] (does not pop)
            if (peek(0).value == 0) {
                throw NullPointerException();
            }
            break;
        }
        case Opcode::LOAD_POINTER: {
            // Stack: [ptr]
            int ptr = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            push(obj.data[0]);
            break;
        }
        case Opcode::STORE_POINTER: {
            // Stack: [value, ptr]
            int ptr = pop().value;
            int val = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            obj.data[0] = val;
            push(val); // Assignment yields value
            break;
        }
        case Opcode::PRINT: {
            int val = pop().value;
            std::cout << val << "\n";
            break;
        }
        case Opcode::POP:
            pop();
            break;
        case Opcode::HALT:
            ip = static_cast<int>(prog.instructions.size()); // force end
            break;
        default:
            throw InvalidInstructionException("Unknown opcode.");
    }
}

} // namespace jscpp
