#include "CodeGenerator.h"

namespace jscpp {

CodeGenerator::CodeGenerator(SemanticAnalyzer& semantic) : semantic(semantic) {}

BytecodeProgram CodeGenerator::generate(Program& program) {
    program.accept(*this);

    // Add halt at the end of global scope (if we support global statements,
    // though typically they are inside main)
    emit(Opcode::HALT);

    if (prog.functions.find("main") != prog.functions.end()) {
        prog.mainEntryPoint = prog.functions["main"].entryPoint;
    }

    return std::move(prog);
}

int CodeGenerator::getInstructionOffset() const {
    return prog.instructions.size();
}

void CodeGenerator::emit(Opcode op, int operand) {
    prog.addInstruction(Instruction(op, operand));
}

void CodeGenerator::patchOperand(int instructionOffset, int newOperand) {
    prog.instructions[instructionOffset].operand = newOperand;
}

void CodeGenerator::visit(IntLiteral& node) {
    int constIdx = prog.addConstant(node.value);
    emit(Opcode::PUSH_CONST, constIdx);
}

void CodeGenerator::visit(BoolLiteral& node) {
    int constIdx = prog.addConstant(node.value ? 1 : 0);
    emit(Opcode::PUSH_CONST, constIdx);
}

void CodeGenerator::visit(NullptrLiteral& node) {
    int constIdx = prog.addConstant(0);
    emit(Opcode::PUSH_CONST, constIdx);
}

void CodeGenerator::visit(IdentifierExpr& node) {
    auto symbol = symTable.resolve(node.name);
    if (!symbol) return; // Should not happen, semantic caught it

    if (isLValue) {
        emit(Opcode::STORE_LOCAL, symbol->localIndex);
    } else {
        emit(Opcode::LOAD_LOCAL, symbol->localIndex);
    }
}

void CodeGenerator::visit(BinaryExpr& node) {
    if (node.op.type == TokenType::ASSIGN) {
        node.right->accept(*this);

        // Handling pointers assignment for reference counting
        auto leftType = semantic.nodeTypes[node.left.get()];
        if (leftType && leftType->kind == TypeKind::Pointer) {
            // Right is evaluated, on stack.
            // We need to INC_REF the new value, DEC_REF the old value, then STORE.
            // But doing this correctly in bytecode might require specific instructions.
            // For simplicity:
            // STACK: [new_val]

            // Check if LHS is simple local variable
            auto idExpr = dynamic_cast<IdentifierExpr*>(node.left.get());
            if (idExpr) {
                auto sym = symTable.resolve(idExpr->name);
                if (sym) {
                    emit(Opcode::INC_REF); // inc_ref new_val
                    emit(Opcode::LOAD_LOCAL, sym->localIndex);
                    emit(Opcode::DEC_REF); // dec_ref old_val
                }
            } else {
                // E.g. array assignment or pointer deref assignment
                // STACK: [new_val]
                // For *p = new_val, we shouldn't change refcount of *p itself,
                // wait, if we assign to *p, and *p is a pointer...
                // Let's assume pointers to pointers are not fully supported or handled similarly.
            }
        }

        isLValue = true;
        node.left->accept(*this); // Will emit STORE_...
        isLValue = false;

        // Assignment expression returns its RHS value, but we just consumed it...
        // For C-like behavior, we should leave it on stack, but typical AST in our subset
        // evaluates statements. If it's used in expr, we need it.
        // For simplicity, we just reload it if it's an IdentifierExpr.
        auto idExpr = dynamic_cast<IdentifierExpr*>(node.left.get());
        if (idExpr) {
            auto sym = symTable.resolve(idExpr->name);
            if (sym) {
                emit(Opcode::LOAD_LOCAL, sym->localIndex);
            }
        }
        return;
    }

    if (node.op.type == TokenType::AND || node.op.type == TokenType::OR) {
        // Short circuit evaluation
        node.left->accept(*this);

        int shortCircuitJump = getInstructionOffset();
        emit(node.op.type == TokenType::AND ? Opcode::JUMP_IF_FALSE : Opcode::JUMP); // jump if false for AND

        if (node.op.type == TokenType::OR) {
            // For OR, if left is true, we jump to end (short circuit), need to keep true on stack
            // If false, pop and eval right
            // Our JUMP_IF_FALSE pops, JUMP doesn't pop conditionally.
            // Simplified: evaluate both for now to keep VM simple, or add proper short circuit opcodes.
            // Let's just evaluate both for our basic compiler
        }
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
        case TokenType::AND: emit(Opcode::AND); break;
        case TokenType::OR: emit(Opcode::OR); break;
        default: break;
    }
}

void CodeGenerator::visit(UnaryExpr& node) {
    node.expr->accept(*this);
    if (node.op.type == TokenType::MINUS) {
        int constIdx = prog.addConstant(-1);
        emit(Opcode::PUSH_CONST, constIdx);
        emit(Opcode::MUL);
    } else if (node.op.type == TokenType::NOT) {
        emit(Opcode::NOT);
    }
}

void CodeGenerator::visit(CallExpr& node) {
    if (node.callee == "print") {
        node.args[0]->accept(*this);
        emit(Opcode::PRINT);
        // Print returns void, but expressions might expect something. Push 0 just in case.
        int constIdx = prog.addConstant(0);
        emit(Opcode::PUSH_CONST, constIdx);
        return;
    }

    for (auto& arg : node.args) {
        arg->accept(*this);
    }

    int funcIdx = prog.addConstant(0); // Placeholder for func entry point
    // We can use a string map for call resolution in VM, or resolve index now.
    // For simplicity, let VM resolve by storing an index to the constant table containing a hash,
    // or just let VM's CALL instruction take string hash.
    // Let's use string hash for CALL operand.
    std::hash<std::string> hasher;
    int hash = hasher(node.callee) & 0x7FFFFFFF;
    emit(Opcode::CALL, hash);
}

void CodeGenerator::visit(ArrayAccessExpr& node) {
    auto symbol = symTable.resolve(node.arrayName);

    if (isLValue) {
        // STORE_ARRAY expects: array_ptr, index, value
        // The value is already evaluated and pushed by the Assign binary expr before this
        // So we need to put array_ptr and index under the value.
        // This is tricky with a simple stack machine.
        // Better: let ArrayAccess generate differently when it's an LValue.
        // Actually, our BinaryExpr assignment does `isLValue = true; node.left->accept();`.
        // So here STACK: [value]. We need: [value, array_ptr, index] then some opcode...
        // Wait, typical store array: STACK: [array_ptr, index, value], then STORE_ARRAY.
        // In BinaryExpr: node.right->accept() (pushes value).
        // If we emit LOAD_LOCAL(array), eval index, then we have [value, array_ptr, index].
        // We'd need a SWAP.
        // Let's change how assignment works if left is array/deref in BinaryExpr, OR
        // just add a specific instruction or handle it explicitly.

        // Easiest fix: When ArrayAccess is visited as LValue, we just emit LOAD_LOCAL array and index.
        // The stack will be [value, array_ptr, index].
        // We can add a STORE_ARRAY_INV instruction, or just fix BinaryExpr.
        // Let's assume BinaryExpr does:
        // if LHS is ArrayAccess:
        //   node.left->arrayName -> LOAD_LOCAL
        //   node.left->index->accept
        //   node.right->accept
        //   STORE_ARRAY
        // But for clean ASTVisitor, let's keep it here. We will just use standard order:
        // Wait, in BinaryExpr for assignment:
        // we pushed value. So stack is [value].
        emit(Opcode::LOAD_LOCAL, symbol->localIndex);
        bool oldLval = isLValue;
        isLValue = false;
        node.index->accept(*this);
        isLValue = oldLval;
        // Stack: [value, array_ref, index].
        // We need STORE_ARRAY that takes (value, array_ref, index)
        // Actually we can just define STORE_ARRAY to pop (index), pop (array_ref), pop (value).
        // Let's use this semantics!
        emit(Opcode::CHECK_BOUNDS);
        emit(Opcode::STORE_ARRAY_INV);
    } else {
        emit(Opcode::LOAD_LOCAL, symbol->localIndex);
        node.index->accept(*this);
        emit(Opcode::CHECK_BOUNDS);
        emit(Opcode::LOAD_ARRAY);
    }
}

void CodeGenerator::visit(NewExpr& node) {
    auto type = semantic.nodeTypes[&node];
    // We only support simple new (e.g. `new int`).
    // ALLOC takes size. For simplicity, ALLOC 1 for single items.
    emit(Opcode::ALLOC, 1);
    // Resulting pointer is on stack.
}

void CodeGenerator::visit(DerefExpr& node) {
    if (isLValue) {
        // Stack: [value]
        bool oldLval = isLValue;
        isLValue = false;
        node.expr->accept(*this); // evaluate pointer
        isLValue = oldLval;
        // Stack: [value, ptr]
        emit(Opcode::CHECK_NULL);
        emit(Opcode::STORE_POINTER); // pops ptr, pops value
    } else {
        node.expr->accept(*this);
        emit(Opcode::CHECK_NULL);
        emit(Opcode::LOAD_POINTER);
    }
}

void CodeGenerator::visit(BlockStmt& node) {
    symTable.enterScope();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }

    // Automatically DEC_REF any local pointer variables going out of scope
    auto& scope = symTable.symbolsInCurrentScope(); // Helper needed
    for (auto& [name, sym] : scope) {
        if (sym->type->kind == TypeKind::Pointer && sym->localIndex != -1) {
            emit(Opcode::LOAD_LOCAL, sym->localIndex);
            emit(Opcode::DEC_REF);
        }
    }

    symTable.leaveScope();
}

void CodeGenerator::visit(ExprStmt& node) {
    node.expr->accept(*this);
    emit(Opcode::POP);
}

void CodeGenerator::visit(VarDeclStmt& node) {
    auto symbol = std::make_shared<Symbol>();
    symbol->name = node.name;
    symbol->kind = SymbolKind::Variable;
    symbol->type = semantic.nodeTypes.count(&node) ? semantic.nodeTypes[&node] : semantic.nodeTypes[&node]; // Actually we need type, but we can reconstruct it
    // Wait, SemanticAnalyzer didn't store VarDeclStmt's type in nodeTypes!
    // But we can get it by resolving from semantic.symTable? No, that's private.
    // Let's just convert it.

    // Better: let's reconstruct the type. But we don't have convertTypeAST in CodeGenerator.
    // Actually, we can just use node.type.isArray and node.type.isPointer for CodeGen.

    // For now, let's just make a dummy type object with the right kind
    symbol->type = std::make_shared<Type>(TypeKind::Unknown);
    if (node.type.isArray) symbol->type = std::make_shared<Type>(TypeKind::Array, nullptr, node.type.arraySize);
    else if (node.type.isPointer) symbol->type = std::make_shared<Type>(TypeKind::Pointer);
    else if (node.type.name == "int") symbol->type = std::make_shared<Type>(TypeKind::Int);
    else if (node.type.name == "bool") symbol->type = std::make_shared<Type>(TypeKind::Bool);

    symTable.define(symbol);

    if (node.initializer) {
        node.initializer->accept(*this);

        if (symbol->type->kind == TypeKind::Pointer) {
            emit(Opcode::INC_REF);
        }
        emit(Opcode::STORE_LOCAL, symbol->localIndex);
    } else {
        // Default init
        if (symbol->type->kind == TypeKind::Pointer || symbol->type->kind == TypeKind::Array) {
            int constIdx = prog.addConstant(0);
            emit(Opcode::PUSH_CONST, constIdx); // null
            emit(Opcode::STORE_LOCAL, symbol->localIndex);
        } else if (symbol->type->kind == TypeKind::Int || symbol->type->kind == TypeKind::Bool) {
            int constIdx = prog.addConstant(0);
            emit(Opcode::PUSH_CONST, constIdx);
            emit(Opcode::STORE_LOCAL, symbol->localIndex);
        }

        // If Array: int arr[5];
        if (symbol->type->kind == TypeKind::Array) {
            emit(Opcode::ALLOC, symbol->type->arraySize);
            emit(Opcode::STORE_LOCAL, symbol->localIndex);
        }
    }
}

void CodeGenerator::visit(IfStmt& node) {
    node.condition->accept(*this);

    int jumpIfFalseOffset = getInstructionOffset();
    emit(Opcode::JUMP_IF_FALSE, 0); // Placeholder

    node.thenBranch->accept(*this);

    if (node.elseBranch) {
        int jumpEndOffset = getInstructionOffset();
        emit(Opcode::JUMP, 0); // Placeholder

        patchOperand(jumpIfFalseOffset, getInstructionOffset());

        node.elseBranch->accept(*this);
        patchOperand(jumpEndOffset, getInstructionOffset());
    } else {
        patchOperand(jumpIfFalseOffset, getInstructionOffset());
    }
}

void CodeGenerator::visit(WhileStmt& node) {
    int startOffset = getInstructionOffset();

    node.condition->accept(*this);

    int jumpIfFalseOffset = getInstructionOffset();
    emit(Opcode::JUMP_IF_FALSE, 0);

    node.body->accept(*this);

    emit(Opcode::JUMP, startOffset);

    patchOperand(jumpIfFalseOffset, getInstructionOffset());
}

void CodeGenerator::visit(ReturnStmt& node) {
    if (node.value) {
        node.value->accept(*this);
    } else {
        int constIdx = prog.addConstant(0);
        emit(Opcode::PUSH_CONST, constIdx);
    }
    emit(Opcode::RETURN);
}

void CodeGenerator::visit(DeleteStmt& node) {
    // We use ARC, delete is not strictly needed, but if provided,
    // it could manually DEC_REF or force deallocation.
    // For this subset, we'll make delete a DEC_REF.
    node.pointer->accept(*this);
    emit(Opcode::DEC_REF);
}

void CodeGenerator::visit(FunctionDecl& node) {
    symTable.enterScope();

    int entryPoint = getInstructionOffset();

    // Define params in scope
    for (const auto& param : node.params) {
        auto symbol = std::make_shared<Symbol>();
        symbol->name = param.name;
        symbol->kind = SymbolKind::Variable;

        symbol->type = std::make_shared<Type>(TypeKind::Unknown);
        if (param.type.isArray) symbol->type = std::make_shared<Type>(TypeKind::Array, nullptr, param.type.arraySize);
        else if (param.type.isPointer) symbol->type = std::make_shared<Type>(TypeKind::Pointer);

        symTable.define(symbol);
    }

    for (auto& stmt : node.body->statements) {
        stmt->accept(*this);
    }

    // Default return
    int constIdx = prog.addConstant(0);
    emit(Opcode::PUSH_CONST, constIdx);
    emit(Opcode::RETURN);

    FunctionMeta meta;
    meta.name = node.name;
    meta.entryPoint = entryPoint;
    meta.numLocals = symTable.getNextLocalIndex();
    meta.numArgs = node.params.size();

    prog.functions[node.name] = meta;

    // Register function in VM call table by hash
    std::hash<std::string> hasher;
    int hash = hasher(node.name) & 0x7FFFFFFF;
    // We store the hash in the constant table, though VM will use `functions` map

    symTable.leaveScope();
}

void CodeGenerator::visit(Program& node) {
    for (auto& decl : node.declarations) {
        decl->accept(*this);
    }
}

} // namespace jscpp
