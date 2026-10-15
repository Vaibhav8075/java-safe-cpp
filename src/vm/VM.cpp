#include "VM.h"
#include "RuntimeError.h"
#include <iostream>
#include <iomanip>

namespace jscpp {

VM::VM(const BytecodeProgram& program, bool trace)
    : prog(program), trace(trace) {

    if (prog.mainEntryPoint == -1) {
        throw RuntimeError("No main function found.");
    }
}

void VM::push(RuntimeValue v) {
    stack.push_back(v);
}

RuntimeValue VM::pop() {
    if (stack.empty()) {
        throw RuntimeError("Operand stack underflow.");
    }
    RuntimeValue v = stack.back();
    stack.pop_back();
    return v;
}

void VM::run() {
    // Setup initial frame
    auto mainFuncIt = prog.functions.find("main");
    if (mainFuncIt == prog.functions.end()) {
        throw RuntimeError("main not found in bytecode meta.");
    }

    const FunctionMeta& mainMeta = mainFuncIt->second;

    ip = prog.mainEntryPoint;

    callStack.push_back({-1, 0});
    locals.resize(mainMeta.numLocals);

    try {
        while (ip >= 0 && ip < (int)prog.instructions.size()) {
            Instruction inst = prog.instructions[ip++];

            if (trace) {
                std::cout << "IP=" << std::setw(4) << std::setfill('0') << (ip - 1)
                          << "  " << inst.toString() << "\n";
            }

            executeInstruction(inst);

            if (inst.op == Opcode::HALT) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Runtime Error:\n" << e.what() << "\n";
    }

    heap.cleanup();
}

void VM::executeInstruction(const Instruction& inst) {
    switch (inst.op) {
        case Opcode::PUSH_CONST:
            push(RuntimeValue(prog.constantTable[inst.operand]));
            break;
        case Opcode::LOAD_LOCAL:
            push(locals[callStack.back().localBase + inst.operand]);
            break;
        case Opcode::STORE_LOCAL:
            locals[callStack.back().localBase + inst.operand] = pop();
            break;
        case Opcode::ADD: {
            int b = pop().value;
            int a = pop().value;
            push(a + b);
            break;
        }
        case Opcode::SUB: {
            int b = pop().value;
            int a = pop().value;
            push(a - b);
            break;
        }
        case Opcode::MUL: {
            int b = pop().value;
            int a = pop().value;
            push(a * b);
            break;
        }
        case Opcode::DIV: {
            int b = pop().value;
            if (b == 0) throw DivisionByZeroException();
            int a = pop().value;
            push(a / b);
            break;
        }
        case Opcode::MOD: {
            int b = pop().value;
            if (b == 0) throw DivisionByZeroException();
            int a = pop().value;
            push(a % b);
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
            // Find function by hash (which is the operand)
            // For simplicity in VM, we find it in the map
            // Since map is string -> Meta, we iterate (slow, but fine for prototype)
            const FunctionMeta* targetMeta = nullptr;
            std::hash<std::string> hasher;
            for (const auto& [name, meta] : prog.functions) {
                if ((hasher(name) & 0x7FFFFFFF) == inst.operand) {
                    targetMeta = &meta;
                    break;
                }
            }

            if (!targetMeta) {
                throw RuntimeError("Function not found.");
            }

            if (callStack.size() >= MAX_CALL_STACK) {
                throw StackOverflowException();
            }

            // Pop arguments to new local space
            int newLocalBase = locals.size();
            locals.resize(locals.size() + targetMeta->numLocals);

            // Args are pushed in order: a, b. Stack top is last arg.
            for (int i = targetMeta->numArgs - 1; i >= 0; --i) {
                locals[newLocalBase + i] = pop();
            }

            callStack.push_back({ip, newLocalBase});
            ip = targetMeta->entryPoint;
            break;
        }
        case Opcode::RETURN: {
            RuntimeValue retVal = pop(); // Assume 1 return value

            Frame frame = callStack.back();
            callStack.pop_back();

            locals.resize(frame.localBase);
            ip = frame.returnAddress;

            push(retVal);

            if (callStack.empty()) {
                // Return from main
                ip = prog.instructions.size(); // end execution
            }
            break;
        }
        case Opcode::ALLOC: {
            int ptr = heap.allocate(inst.operand);
            push(ptr);
            break;
        }
        case Opcode::INC_REF: {
            int ptr = pop().value;
            heap.incRef(ptr);
            push(ptr);
            break;
        }
        case Opcode::DEC_REF: {
            int ptr = pop().value;
            heap.decRef(ptr);
            break;
        }
        case Opcode::LOAD_ARRAY: {
            // Stack: [ptr, index]
            int index = pop().value;
            int ptr = pop().value;
            // CHECK_BOUNDS is separate, but we do actual access here
            ManagedObject& obj = heap.getObject(ptr);
            push(obj.data[index]);
            break;
        }
        case Opcode::STORE_ARRAY: {
            // Stack: [ptr, index, value]
            int val = pop().value;
            int index = pop().value;
            int ptr = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            obj.data[index] = val;
            break;
        }
        case Opcode::STORE_ARRAY_INV: {
            // Stack: [value, ptr, index]
            int index = pop().value;
            int ptr = pop().value;
            int val = pop().value;
            ManagedObject& obj = heap.getObject(ptr);
            obj.data[index] = val;
            push(val); // Assignment returns RHS value
            break;
        }
        case Opcode::CHECK_BOUNDS: {
            // Stack: [..., ptr, index] (does not pop)
            int index = stack.back().value;
            int ptr = stack[stack.size() - 2].value;
            ManagedObject& obj = heap.getObject(ptr);
            if (index < 0 || index >= obj.size) {
                throw ArrayIndexOutOfBoundsException(index, obj.size);
            }
            break;
        }
        case Opcode::CHECK_NULL: {
            // Stack: [..., ptr] (does not pop)
            int ptr = stack.back().value;
            if (ptr == 0) {
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
            std::cout << val << std::endl;
            break;
        }
        case Opcode::POP:
            pop();
            break;
        case Opcode::HALT:
            ip = prog.instructions.size(); // force end
            break;
        default:
            throw InvalidInstructionException("Unknown opcode.");
    }
}

} // namespace jscpp
