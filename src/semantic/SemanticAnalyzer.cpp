#include "SemanticAnalyzer.h"
#include "common/Error.h"
#include <iostream>

namespace jscpp {

SemanticAnalyzer::SemanticAnalyzer() {
    // Add print function to global scope
    auto printFunc = std::make_shared<Symbol>();
    printFunc->name = "print";
    printFunc->kind = SymbolKind::Function;
    printFunc->type = std::make_shared<Type>(TypeKind::Void);
    printFunc->paramTypes.push_back(std::make_shared<Type>(TypeKind::Int)); // simple int print
    symTable.define(printFunc);
}

std::shared_ptr<Type> SemanticAnalyzer::convertTypeAST(const TypeAST& typeAST) {
    std::shared_ptr<Type> baseType;
    if (typeAST.name == "int") baseType = std::make_shared<Type>(TypeKind::Int);
    else if (typeAST.name == "bool") baseType = std::make_shared<Type>(TypeKind::Bool);
    else if (typeAST.name == "void") baseType = std::make_shared<Type>(TypeKind::Void);
    else throw SemanticError("Unknown type name: " + typeAST.name, SourceLocation()); // Loc handled better contextually

    if (typeAST.isArray) {
        return std::make_shared<Type>(TypeKind::Array, baseType, typeAST.arraySize);
    } else if (typeAST.isPointer) {
        return std::make_shared<Type>(TypeKind::Pointer, baseType);
    }

    return baseType;
}

void SemanticAnalyzer::checkType(const std::shared_ptr<Type>& expected, const std::shared_ptr<Type>& actual, const SourceLocation& loc) {
    if (expected->kind == TypeKind::Pointer && actual->kind == TypeKind::Nullptr_t) return; // nullptr is assignable to any pointer
    if (*expected != *actual) {
        throw SemanticError("Type mismatch. Expected " + expected->toString() + ", got " + actual->toString(), loc);
    }
}

bool SemanticAnalyzer::isNumeric(const std::shared_ptr<Type>& type) {
    return type->kind == TypeKind::Int;
}

bool SemanticAnalyzer::isBoolean(const std::shared_ptr<Type>& type) {
    return type->kind == TypeKind::Bool;
}

bool SemanticAnalyzer::isPointer(const std::shared_ptr<Type>& type) {
    return type->kind == TypeKind::Pointer || type->kind == TypeKind::Nullptr_t;
}

bool SemanticAnalyzer::isArray(const std::shared_ptr<Type>& type) {
    return type->kind == TypeKind::Array;
}

void SemanticAnalyzer::analyze(Program& program) {
    program.accept(*this);
}

void SemanticAnalyzer::visit(IntLiteral& node) {
    nodeTypes[&node] = std::make_shared<Type>(TypeKind::Int);
}

void SemanticAnalyzer::visit(BoolLiteral& node) {
    nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
}

void SemanticAnalyzer::visit(NullptrLiteral& node) {
    nodeTypes[&node] = std::make_shared<Type>(TypeKind::Nullptr_t);
}

void SemanticAnalyzer::visit(IdentifierExpr& node) {
    auto symbol = symTable.resolve(node.name);
    if (!symbol) {
        throw SemanticError("Undeclared variable: " + node.name, node.loc);
    }
    nodeTypes[&node] = symbol->type;
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    node.left->accept(*this);
    node.right->accept(*this);

    auto leftType = nodeTypes[node.left.get()];
    auto rightType = nodeTypes[node.right.get()];

    if (node.op.type == TokenType::ASSIGN) {
        checkType(leftType, rightType, node.loc);
        nodeTypes[&node] = leftType;
        return;
    }

    if (node.op.type == TokenType::PLUS || node.op.type == TokenType::MINUS ||
        node.op.type == TokenType::STAR || node.op.type == TokenType::SLASH ||
        node.op.type == TokenType::PERCENT) {
        if (!isNumeric(leftType) || !isNumeric(rightType)) {
            throw SemanticError("Operands to arithmetic operator must be numeric.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Int);
        return;
    }

    if (node.op.type == TokenType::EQ || node.op.type == TokenType::NEQ) {
        if (*leftType != *rightType && !(isPointer(leftType) && isPointer(rightType))) {
            throw SemanticError("Operands to equality operator must have same type.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
        return;
    }

    if (node.op.type == TokenType::LT || node.op.type == TokenType::LTE ||
        node.op.type == TokenType::GT || node.op.type == TokenType::GTE) {
        if (!isNumeric(leftType) || !isNumeric(rightType)) {
            throw SemanticError("Operands to relational operator must be numeric.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
        return;
    }

    if (node.op.type == TokenType::AND || node.op.type == TokenType::OR) {
        if (!isBoolean(leftType) || !isBoolean(rightType)) {
            throw SemanticError("Operands to logical operator must be boolean.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
        return;
    }

    throw SemanticError("Unknown binary operator.", node.loc);
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    node.expr->accept(*this);
    auto exprType = nodeTypes[node.expr.get()];

    if (node.op.type == TokenType::MINUS) {
        if (!isNumeric(exprType)) {
            throw SemanticError("Operand to unary minus must be numeric.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Int);
    } else if (node.op.type == TokenType::NOT) {
        if (!isBoolean(exprType)) {
            throw SemanticError("Operand to logical not must be boolean.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
    } else {
        throw SemanticError("Unknown unary operator.", node.loc);
    }
}

void SemanticAnalyzer::visit(CallExpr& node) {
    auto symbol = symTable.resolve(node.callee);
    if (!symbol || symbol->kind != SymbolKind::Function) {
        throw SemanticError("Call to undeclared function: " + node.callee, node.loc);
    }

    if (symbol->paramTypes.size() != node.args.size()) {
        throw SemanticError("Incorrect number of arguments to function " + node.callee, node.loc);
    }

    for (size_t i = 0; i < node.args.size(); ++i) {
        node.args[i]->accept(*this);
        checkType(symbol->paramTypes[i], nodeTypes[node.args[i].get()], node.args[i]->loc);
    }

    nodeTypes[&node] = symbol->type;
}

void SemanticAnalyzer::visit(ArrayAccessExpr& node) {
    auto symbol = symTable.resolve(node.arrayName);
    if (!symbol || !isArray(symbol->type)) {
        throw SemanticError("Array access on non-array type: " + node.arrayName, node.loc);
    }

    node.index->accept(*this);
    if (!isNumeric(nodeTypes[node.index.get()])) {
        throw SemanticError("Array index must be numeric.", node.loc);
    }

    nodeTypes[&node] = symbol->type->baseType;
}

void SemanticAnalyzer::visit(NewExpr& node) {
    TypeAST typeAst(node.typeName.lexeme);
    auto type = convertTypeAST(typeAst);
    nodeTypes[&node] = std::make_shared<Type>(TypeKind::Pointer, type);
}

void SemanticAnalyzer::visit(DerefExpr& node) {
    node.expr->accept(*this);
    auto exprType = nodeTypes[node.expr.get()];

    if (!isPointer(exprType) || exprType->kind == TypeKind::Nullptr_t) {
        throw SemanticError("Cannot dereference non-pointer type.", node.loc);
    }

    nodeTypes[&node] = exprType->baseType;
}

void SemanticAnalyzer::visit(BlockStmt& node) {
    symTable.enterScope();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    symTable.leaveScope();
}

void SemanticAnalyzer::visit(ExprStmt& node) {
    node.expr->accept(*this);
}

void SemanticAnalyzer::visit(VarDeclStmt& node) {
    auto type = convertTypeAST(node.type);

    if (type->kind == TypeKind::Void) {
        throw SemanticError("Cannot declare variable of type void.", node.loc);
    }

    if (node.initializer) {
        node.initializer->accept(*this);
        checkType(type, nodeTypes[node.initializer.get()], node.loc);
    }

    auto symbol = std::make_shared<Symbol>();
    symbol->name = node.name;
    symbol->kind = SymbolKind::Variable;
    symbol->type = type;

    if (!symTable.define(symbol)) {
        throw SemanticError("Variable already declared in this scope: " + node.name, node.loc);
    }
}

void SemanticAnalyzer::visit(IfStmt& node) {
    node.condition->accept(*this);
    if (!isBoolean(nodeTypes[node.condition.get()])) {
        throw SemanticError("If condition must be boolean.", node.loc);
    }

    node.thenBranch->accept(*this);
    if (node.elseBranch) {
        node.elseBranch->accept(*this);
    }
}

void SemanticAnalyzer::visit(WhileStmt& node) {
    node.condition->accept(*this);
    if (!isBoolean(nodeTypes[node.condition.get()])) {
        throw SemanticError("While condition must be boolean.", node.loc);
    }

    node.body->accept(*this);
}

void SemanticAnalyzer::visit(ReturnStmt& node) {
    if (node.value) {
        node.value->accept(*this);
        checkType(currentReturnType, nodeTypes[node.value.get()], node.loc);
    } else {
        if (currentReturnType->kind != TypeKind::Void) {
            throw SemanticError("Return value required for non-void function.", node.loc);
        }
    }
}

void SemanticAnalyzer::visit(DeleteStmt& node) {
    node.pointer->accept(*this);
    auto pType = nodeTypes[node.pointer.get()];
    if (!isPointer(pType)) {
        throw SemanticError("Can only delete pointers.", node.loc);
    }
}

void SemanticAnalyzer::visit(FunctionDecl& node) {
    auto retType = convertTypeAST(node.returnType);

    auto symbol = std::make_shared<Symbol>();
    symbol->name = node.name;
    symbol->kind = SymbolKind::Function;
    symbol->type = retType;

    for (const auto& param : node.params) {
        symbol->paramTypes.push_back(convertTypeAST(param.type));
    }

    if (!symTable.define(symbol)) {
        throw SemanticError("Function already declared: " + node.name, node.loc);
    }

    symTable.enterScope();
    currentReturnType = retType;

    for (const auto& param : node.params) {
        auto paramSymbol = std::make_shared<Symbol>();
        paramSymbol->name = param.name;
        paramSymbol->kind = SymbolKind::Variable;
        paramSymbol->type = convertTypeAST(param.type);
        symTable.define(paramSymbol);
    }

    // Check body without entering new scope because params are in function scope
    for (auto& stmt : node.body->statements) {
        stmt->accept(*this);
    }

    symTable.leaveScope();
}

void SemanticAnalyzer::visit(Program& node) {
    for (auto& decl : node.declarations) {
        decl->accept(*this);
    }
}

} // namespace jscpp
