#include "CodeGenerator.h"
#include <stdexcept>

namespace jscpp {

// Reference-counting convention used by the generated code:
//  - Every local pointer or array variable owns one reference to its heap object.
//  - "new" and calls returning a pointer produce an *owned* temporary (refcount already +1),
//    so storing them transfers ownership instead of adding another reference.
//  - Reading a variable produces a *borrowed* value; storing it adds a reference (INC_REF).
//  - Pointer arguments are handed to the callee as owned references; the callee releases its
//    parameters when it returns, like any other local.
//  - Leaving a block, returning, or reassigning a variable releases the old reference (DEC_REF).

namespace {
// Sets the source line recorded on emitted instructions for the lifetime of the guard
struct LineGuard {
    int& line;
    int saved;
    LineGuard(int& line, int newLine) : line(line), saved(line) {
        if (newLine > 0) line = newLine;
    }
    ~LineGuard() { line = saved; }
};
}

CodeGenerator::CodeGenerator(SemanticAnalyzer& semantic) : semantic(semantic) {}

BytecodeProgram CodeGenerator::generate(Program& program) {
    program.accept(*this);
    emit(Opcode::HALT);

    int mainIdx = prog.findFunction("main");
    if (mainIdx >= 0) {
        prog.mainEntryPoint = prog.functions[mainIdx].entryPoint;
    }
    prog.verify();
    return std::move(prog);
}

int CodeGenerator::getInstructionOffset() const {
    return static_cast<int>(prog.instructions.size());
}

void CodeGenerator::emit(Opcode op, int operand) {
    prog.addInstruction(Instruction(op, operand, currentLine));
}

void CodeGenerator::emitConst(int value) {
    emit(Opcode::PUSH_CONST, prog.addConstant(value));
}

void CodeGenerator::patchOperand(int instructionOffset, int newOperand) {
    prog.instructions[instructionOffset].operand = newOperand;
}

const CodeGenerator::LocalVar& CodeGenerator::resolve(const std::string& name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        for (auto var = scope->rbegin(); var != scope->rend(); ++var) {
            if (var->name == name) return *var;
        }
    }
    throw std::logic_error("codegen: unresolved variable " + name); // semantic analysis prevents this
}

int CodeGenerator::defineLocal(const std::string& name, std::shared_ptr<Type> type) {
    // Every variable in a function gets its own slot, so a slot never holds a stale
    // value from a different variable
    int slot = nextSlot++;
    scopes.back().push_back({name, slot, std::move(type)});
    return slot;
}

bool CodeGenerator::isManaged(const std::shared_ptr<Type>& type) {
    return type && (type->kind == TypeKind::Pointer || type->kind == TypeKind::Array);
}

bool CodeGenerator::isPointerExpr(Expr& expr) {
    auto type = semantic.nodeTypes[&expr];
    return type && (type->kind == TypeKind::Pointer || type->kind == TypeKind::Nullptr_t);
}

bool CodeGenerator::isOwnedTemp(Expr& expr) {
    if (dynamic_cast<NewExpr*>(&expr)) return true;
    if (dynamic_cast<CallExpr*>(&expr)) return isPointerExpr(expr);
    return false;
}

void CodeGenerator::emitRelease(const std::vector<LocalVar>& scope, bool clearSlots) {
    for (auto var = scope.rbegin(); var != scope.rend(); ++var) {
        if (!isManaged(var->type)) continue;
        emit(Opcode::LOAD_LOCAL, var->slot);
        emit(Opcode::DEC_REF);
        if (clearSlots) {
            // Null the slot so a loop that re-enters this block never releases a dead object twice
            emitConst(0);
            emit(Opcode::STORE_LOCAL, var->slot);
        }
    }
}

void CodeGenerator::visit(IntLiteral& node) {
    emitConst(node.value);
}

void CodeGenerator::visit(BoolLiteral& node) {
    emitConst(node.value ? 1 : 0);
}

void CodeGenerator::visit(NullptrLiteral&) {
    emitConst(0);
}

void CodeGenerator::visit(IdentifierExpr& node) {
    emit(Opcode::LOAD_LOCAL, resolve(node.name).slot);
}

// The right operand is evaluated first (C++17 sequences it before the left), then stored.
// The value of the whole assignment expression is left on the stack.
void CodeGenerator::emitAssign(BinaryExpr& node) {
    node.right->accept(*this);

    if (auto id = dynamic_cast<IdentifierExpr*>(node.left.get())) {
        const LocalVar& var = resolve(id->name);
        if (var.type->kind == TypeKind::Pointer) {
            if (!isOwnedTemp(*node.right)) emit(Opcode::INC_REF); // retain new value first (safe for p = p)
            emit(Opcode::LOAD_LOCAL, var.slot);
            emit(Opcode::DEC_REF);                                // then release the old one
        }
        emit(Opcode::STORE_LOCAL, var.slot);
        emit(Opcode::LOAD_LOCAL, var.slot);
    } else if (auto arr = dynamic_cast<ArrayAccessExpr*>(node.left.get())) {
        LineGuard at(currentLine, arr->loc.line);
        emit(Opcode::LOAD_LOCAL, resolve(arr->arrayName).slot);
        arr->index->accept(*this);
        emit(Opcode::CHECK_BOUNDS);
        emit(Opcode::STORE_ARRAY_INV); // pops index, array, value; pushes value
    } else if (auto deref = dynamic_cast<DerefExpr*>(node.left.get())) {
        LineGuard at(currentLine, deref->loc.line);
        deref->expr->accept(*this);
        emit(Opcode::CHECK_NULL);
        emit(Opcode::STORE_POINTER); // pops pointer, value; pushes value
    } else {
        throw std::logic_error("codegen: invalid assignment target"); // semantic analysis prevents this
    }
}

void CodeGenerator::visit(BinaryExpr& node) {
    LineGuard at(currentLine, node.loc.line);

    if (node.op.type == TokenType::ASSIGN) {
        emitAssign(node);
        return;
    }

    if (node.op.type == TokenType::AND) {
        // left && right: skip right when left is false
        node.left->accept(*this);
        int jumpToFalse = getInstructionOffset();
        emit(Opcode::JUMP_IF_FALSE, 0);
        node.right->accept(*this);
        int jumpToEnd = getInstructionOffset();
        emit(Opcode::JUMP, 0);
        patchOperand(jumpToFalse, getInstructionOffset());
        emitConst(0);
        patchOperand(jumpToEnd, getInstructionOffset());
        return;
    }

    if (node.op.type == TokenType::OR) {
        // left || right: skip right when left is true
        node.left->accept(*this);
        int jumpToRight = getInstructionOffset();
        emit(Opcode::JUMP_IF_FALSE, 0);
        emitConst(1);
        int jumpToEnd = getInstructionOffset();
        emit(Opcode::JUMP, 0);
        patchOperand(jumpToRight, getInstructionOffset());
        node.right->accept(*this);
        patchOperand(jumpToEnd, getInstructionOffset());
        return;
    }

    node.left->accept(*this);
    node.right->accept(*this);

    switch (node.op.type) {
        case TokenType::PLUS: emit(Opcode::ADD); break;
        case TokenType::MINUS: emit(Opcode::SUB); break;
        case TokenType::STAR: emit(Opcode::MUL); break;
        case TokenType::SLASH: emit(Opcode::DIV); break;
        case TokenType::PERCENT: emit(Opcode::MOD); break;
        case TokenType::EQ: emit(Opcode::EQ); break;
        case TokenType::NEQ: emit(Opcode::NE); break;
        case TokenType::LT: emit(Opcode::LT); break;
        case TokenType::GT: emit(Opcode::GT); break;
        case TokenType::LTE: emit(Opcode::LE); break;
        case TokenType::GTE: emit(Opcode::GE); break;
        default: throw std::logic_error("codegen: unknown binary operator " + node.op.lexeme);
    }
}

void CodeGenerator::visit(UnaryExpr& node) {
    node.expr->accept(*this);
    if (node.op.type == TokenType::MINUS) {
        emitConst(-1);
        emit(Opcode::MUL);
    } else if (node.op.type == TokenType::NOT) {
        emit(Opcode::NOT);
    }
}

void CodeGenerator::visit(CallExpr& node) {
    LineGuard at(currentLine, node.loc.line);

    if (node.callee == "print") {
        node.args[0]->accept(*this);
        emit(Opcode::PRINT);
        emitConst(0); // every call leaves one value; ExprStmt pops it
        return;
    }

    const auto& paramTypes = semantic.functions.at(node.callee)->paramTypes;
    for (size_t i = 0; i < node.args.size(); ++i) {
        node.args[i]->accept(*this);
        if (paramTypes[i]->kind == TypeKind::Pointer && !isOwnedTemp(*node.args[i])) {
            emit(Opcode::INC_REF); // the callee's parameter becomes a new owner
        }
    }
    emit(Opcode::CALL, functionIndex.at(node.callee));
}

void CodeGenerator::visit(ArrayAccessExpr& node) {
    LineGuard at(currentLine, node.loc.line);
    emit(Opcode::LOAD_LOCAL, resolve(node.arrayName).slot);
    node.index->accept(*this);
    emit(Opcode::CHECK_BOUNDS);
    emit(Opcode::LOAD_ARRAY);
}

void CodeGenerator::visit(NewExpr& node) {
    LineGuard at(currentLine, node.loc.line);
    emit(Opcode::ALLOC, 1); // new object starts with refcount 1, owned by this expression
}

void CodeGenerator::visit(DerefExpr& node) {
    LineGuard at(currentLine, node.loc.line);
    node.expr->accept(*this);
    emit(Opcode::CHECK_NULL);
    emit(Opcode::LOAD_POINTER);
}

void CodeGenerator::visit(BlockStmt& node) {
    scopes.emplace_back();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    LineGuard at(currentLine, node.loc.line);
    emitRelease(scopes.back(), true);
    scopes.pop_back();
}

void CodeGenerator::visit(ExprStmt& node) {
    LineGuard at(currentLine, node.loc.line);
    node.expr->accept(*this);
    if (isOwnedTemp(*node.expr)) {
        emit(Opcode::DEC_REF); // e.g. "new int;" or "makePtr();" - nobody keeps the result
    } else {
        emit(Opcode::POP);
    }
}

void CodeGenerator::visit(VarDeclStmt& node) {
    LineGuard at(currentLine, node.loc.line);
    auto type = semantic.nodeTypes.at(&node);

    // Evaluate the initializer before the name is in scope ("int x = x;" sees the outer x)
    if (type->kind == TypeKind::Array) {
        emit(Opcode::ALLOC, type->arraySize);
    } else if (node.initializer) {
        node.initializer->accept(*this);
        if (type->kind == TypeKind::Pointer && !isOwnedTemp(*node.initializer)) {
            emit(Opcode::INC_REF);
        }
    } else {
        emitConst(0); // ints, bools and pointers start as 0 / false / nullptr
    }

    int slot = defineLocal(node.name, type);
    if (isManaged(type)) {
        // Release whatever the slot held, in case this declaration runs again inside a loop
        emit(Opcode::LOAD_LOCAL, slot);
        emit(Opcode::DEC_REF);
    }
    emit(Opcode::STORE_LOCAL, slot);
}

void CodeGenerator::visit(IfStmt& node) {
    LineGuard at(currentLine, node.loc.line);
    node.condition->accept(*this);

    int jumpIfFalseOffset = getInstructionOffset();
    emit(Opcode::JUMP_IF_FALSE, 0);

    node.thenBranch->accept(*this);

    if (node.elseBranch) {
        int jumpEndOffset = getInstructionOffset();
        emit(Opcode::JUMP, 0);
        patchOperand(jumpIfFalseOffset, getInstructionOffset());
        node.elseBranch->accept(*this);
        patchOperand(jumpEndOffset, getInstructionOffset());
    } else {
        patchOperand(jumpIfFalseOffset, getInstructionOffset());
    }
}

void CodeGenerator::visit(WhileStmt& node) {
    LineGuard at(currentLine, node.loc.line);
    int startOffset = getInstructionOffset();

    node.condition->accept(*this);

    int jumpIfFalseOffset = getInstructionOffset();
    emit(Opcode::JUMP_IF_FALSE, 0);

    node.body->accept(*this);

    emit(Opcode::JUMP, startOffset);
    patchOperand(jumpIfFalseOffset, getInstructionOffset());
}

void CodeGenerator::visit(ReturnStmt& node) {
    LineGuard at(currentLine, node.loc.line);
    if (node.value) {
        node.value->accept(*this);
        if (currentReturnType->kind == TypeKind::Pointer && !isOwnedTemp(*node.value)) {
            emit(Opcode::INC_REF); // keep the object alive for the caller after locals are released
        }
    } else {
        emitConst(0);
    }
    // Release every local of the function that is in scope at this point
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        emitRelease(*scope, false);
    }
    emit(Opcode::RETURN);
}

void CodeGenerator::visit(DeleteStmt& node) {
    LineGuard at(currentLine, node.loc.line);
    // With ARC, delete just gives up this variable's reference and sets it to nullptr,
    // so it can never leave a dangling pointer behind
    int slot = resolve(static_cast<IdentifierExpr&>(*node.pointer).name).slot;
    emit(Opcode::LOAD_LOCAL, slot);
    emit(Opcode::DEC_REF);
    emitConst(0);
    emit(Opcode::STORE_LOCAL, slot);
}

void CodeGenerator::visit(FunctionDecl& node) {
    LineGuard at(currentLine, node.loc.line);
    auto signature = semantic.functions.at(node.name);
    currentReturnType = signature->type;

    scopes.clear();
    scopes.emplace_back();
    nextSlot = 0;

    int entryPoint = getInstructionOffset();

    // Arguments are copied into slots 0..n-1 by CALL
    for (size_t i = 0; i < node.params.size(); ++i) {
        defineLocal(node.params[i].name, signature->paramTypes[i]);
    }

    for (auto& stmt : node.body->statements) {
        stmt->accept(*this);
    }

    // Implicit return at the end (void functions, and main which returns 0)
    emitRelease(scopes.back(), false);
    emitConst(0);
    emit(Opcode::RETURN);

    FunctionMeta& meta = prog.functions[functionIndex.at(node.name)];
    meta.entryPoint = entryPoint;
    meta.numLocals = nextSlot;
    meta.numArgs = static_cast<int>(node.params.size());

    scopes.clear();
}

void CodeGenerator::visit(Program& node) {
    // Give every function its table index up front so calls can be emitted before the callee
    for (auto& decl : node.declarations) {
        if (auto func = dynamic_cast<FunctionDecl*>(decl.get())) {
            functionIndex[func->name] = static_cast<int>(prog.functions.size());
            FunctionMeta meta;
            meta.name = func->name;
            prog.functions.push_back(meta);
        }
    }
    for (auto& decl : node.declarations) {
        decl->accept(*this);
    }
}

} // namespace jscpp
