#include "SemanticAnalyzer.h"
#include "common/Error.h"

namespace jscpp {

SemanticAnalyzer::SemanticAnalyzer() {
    // Built-in: void print(int)
    auto printFunc = std::make_shared<Symbol>();
    printFunc->name = "print";
    printFunc->kind = SymbolKind::Function;
    printFunc->type = std::make_shared<Type>(TypeKind::Void);
    printFunc->paramTypes.push_back(std::make_shared<Type>(TypeKind::Int));
    symTable.define(printFunc);
    functions["print"] = printFunc;
}

std::shared_ptr<Type> SemanticAnalyzer::convertTypeAST(const TypeAST& typeAST, const SourceLocation& loc) {
    std::shared_ptr<Type> baseType;
    if (typeAST.name == "int") baseType = std::make_shared<Type>(TypeKind::Int);
    else if (typeAST.name == "bool") baseType = std::make_shared<Type>(TypeKind::Bool);
    else if (typeAST.name == "void") baseType = std::make_shared<Type>(TypeKind::Void);
    else throw SemanticError("Unknown type name: " + typeAST.name, loc);

    if (typeAST.isPointer && baseType->kind == TypeKind::Void) {
        throw SemanticError("void* is not supported; use int* or bool*.", loc);
    }

    std::shared_ptr<Type> type = baseType;
    if (typeAST.isPointer) {
        type = std::make_shared<Type>(TypeKind::Pointer, baseType);
    }
    if (typeAST.isArray) {
        if (type->kind != TypeKind::Int && type->kind != TypeKind::Bool) {
            // Heap cells are not reference counted, so storing pointers in them would break ARC
            throw SemanticError("Only arrays of int or bool are supported.", loc);
        }
        type = std::make_shared<Type>(TypeKind::Array, type, typeAST.arraySize);
    }
    return type;
}

std::shared_ptr<Type> SemanticAnalyzer::typeOf(Expr& expr) {
    expr.accept(*this);
    return nodeTypes[&expr];
}

void SemanticAnalyzer::checkType(const std::shared_ptr<Type>& expected, const std::shared_ptr<Type>& actual, const SourceLocation& loc) {
    if (expected->kind == TypeKind::Pointer && actual->kind == TypeKind::Nullptr_t) return; // nullptr is assignable to any pointer
    if (*expected != *actual) {
        throw SemanticError("Type mismatch. Expected " + expected->toString() + ", got " + actual->toString(), loc);
    }
}

// Only real storage locations may appear on the left of '='
void SemanticAnalyzer::checkAssignable(Expr& target) {
    if (auto id = dynamic_cast<IdentifierExpr*>(&target)) {
        auto symbol = symTable.resolve(id->name);
        if (symbol && isArray(symbol->type)) {
            throw SemanticError("Arrays cannot be assigned as a whole; assign elements like " + id->name + "[i].", target.loc);
        }
        return;
    }
    if (dynamic_cast<ArrayAccessExpr*>(&target) || dynamic_cast<DerefExpr*>(&target)) {
        return;
    }
    throw SemanticError("Left side of '=' must be a variable, an array element or *pointer.", target.loc);
}

// Conservative check that a statement cannot complete normally without returning
bool SemanticAnalyzer::alwaysReturns(const Stmt& stmt) {
    if (dynamic_cast<const ReturnStmt*>(&stmt)) return true;
    if (auto block = dynamic_cast<const BlockStmt*>(&stmt)) {
        for (const auto& s : block->statements) {
            if (alwaysReturns(*s)) return true;
        }
        return false;
    }
    if (auto ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        return ifStmt->elseBranch && alwaysReturns(*ifStmt->thenBranch) && alwaysReturns(*ifStmt->elseBranch);
    }
    if (auto whileStmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        // while (true) { ... } never falls through
        auto cond = dynamic_cast<const BoolLiteral*>(whileStmt->condition.get());
        return cond && cond->value;
    }
    return false;
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
    if (symbol->kind == SymbolKind::Function) {
        throw SemanticError("'" + node.name + "' is a function; call it with " + node.name + "(...).", node.loc);
    }
    nodeTypes[&node] = symbol->type;
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    if (node.op.type == TokenType::ASSIGN) {
        auto leftType = typeOf(*node.left);
        checkAssignable(*node.left);
        auto rightType = typeOf(*node.right);
        checkType(leftType, rightType, node.loc);
        nodeTypes[&node] = leftType;
        return;
    }

    auto leftType = typeOf(*node.left);
    auto rightType = typeOf(*node.right);

    if (node.op.type == TokenType::PLUS || node.op.type == TokenType::MINUS ||
        node.op.type == TokenType::STAR || node.op.type == TokenType::SLASH ||
        node.op.type == TokenType::PERCENT) {
        if (!isNumeric(leftType) || !isNumeric(rightType)) {
            // This also rules out pointer arithmetic, which could forge addresses
            throw SemanticError("Operands to arithmetic operator '" + node.op.lexeme + "' must be int (got " +
                                leftType->toString() + " and " + rightType->toString() + ").", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Int);
        return;
    }

    if (node.op.type == TokenType::EQ || node.op.type == TokenType::NEQ) {
        bool bothPointers = isPointer(leftType) && isPointer(rightType);
        if (bothPointers) {
            if (leftType->kind == TypeKind::Pointer && rightType->kind == TypeKind::Pointer && *leftType != *rightType) {
                throw SemanticError("Cannot compare " + leftType->toString() + " with " + rightType->toString() + ".", node.loc);
            }
        } else if (*leftType != *rightType || isArray(leftType)) {
            throw SemanticError("Operands to '" + node.op.lexeme + "' must have the same type (got " +
                                leftType->toString() + " and " + rightType->toString() + ").", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
        return;
    }

    if (node.op.type == TokenType::LT || node.op.type == TokenType::LTE ||
        node.op.type == TokenType::GT || node.op.type == TokenType::GTE) {
        if (!isNumeric(leftType) || !isNumeric(rightType)) {
            throw SemanticError("Operands to relational operator '" + node.op.lexeme + "' must be int.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
        return;
    }

    if (node.op.type == TokenType::AND || node.op.type == TokenType::OR) {
        if (!isBoolean(leftType) || !isBoolean(rightType)) {
            throw SemanticError("Operands to logical operator '" + node.op.lexeme + "' must be bool.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Bool);
        return;
    }

    throw SemanticError("Unknown binary operator.", node.loc);
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    auto exprType = typeOf(*node.expr);

    if (node.op.type == TokenType::MINUS) {
        if (!isNumeric(exprType)) {
            throw SemanticError("Operand to unary minus must be int.", node.loc);
        }
        nodeTypes[&node] = std::make_shared<Type>(TypeKind::Int);
    } else if (node.op.type == TokenType::NOT) {
        if (!isBoolean(exprType)) {
            throw SemanticError("Operand to logical not must be bool.", node.loc);
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
        throw SemanticError("Function " + node.callee + " expects " + std::to_string(symbol->paramTypes.size()) +
                            " argument(s) but got " + std::to_string(node.args.size()) + ".", node.loc);
    }

    for (size_t i = 0; i < node.args.size(); ++i) {
        checkType(symbol->paramTypes[i], typeOf(*node.args[i]), node.args[i]->loc);
    }

    nodeTypes[&node] = symbol->type;
}

void SemanticAnalyzer::visit(ArrayAccessExpr& node) {
    auto symbol = symTable.resolve(node.arrayName);
    if (!symbol || symbol->kind != SymbolKind::Variable) {
        throw SemanticError("Undeclared array: " + node.arrayName, node.loc);
    }
    if (!isArray(symbol->type)) {
        throw SemanticError("'" + node.arrayName + "' is not an array (type " + symbol->type->toString() + ").", node.loc);
    }

    if (!isNumeric(typeOf(*node.index))) {
        throw SemanticError("Array index must be int.", node.loc);
    }

    nodeTypes[&node] = symbol->type->baseType;
}

void SemanticAnalyzer::visit(NewExpr& node) {
    TypeAST typeAst(node.typeName.lexeme);
    auto type = convertTypeAST(typeAst, node.loc);
    if (type->kind == TypeKind::Void) {
        throw SemanticError("Cannot allocate 'new void'.", node.loc);
    }
    nodeTypes[&node] = std::make_shared<Type>(TypeKind::Pointer, type);
}

void SemanticAnalyzer::visit(DerefExpr& node) {
    auto exprType = typeOf(*node.expr);

    if (exprType->kind == TypeKind::Nullptr_t) {
        throw SemanticError("Dereferencing nullptr is always an error.", node.loc);
    }
    if (exprType->kind != TypeKind::Pointer) {
        throw SemanticError("Cannot dereference non-pointer type " + exprType->toString() + ".", node.loc);
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
    auto type = convertTypeAST(node.type, node.loc);

    if (type->kind == TypeKind::Void) {
        throw SemanticError("Cannot declare variable of type void.", node.loc);
    }

    if (node.initializer) {
        checkType(type, typeOf(*node.initializer), node.loc);
    }

    auto symbol = std::make_shared<Symbol>();
    symbol->name = node.name;
    symbol->kind = SymbolKind::Variable;
    symbol->type = type;

    if (!symTable.define(symbol)) {
        throw SemanticError("Variable already declared in this scope: " + node.name, node.loc);
    }
    nodeTypes[&node] = type;
}

void SemanticAnalyzer::visit(IfStmt& node) {
    if (!isBoolean(typeOf(*node.condition))) {
        throw SemanticError("If condition must be bool (write x != 0 for an int).", node.loc);
    }

    node.thenBranch->accept(*this);
    if (node.elseBranch) {
        node.elseBranch->accept(*this);
    }
}

void SemanticAnalyzer::visit(WhileStmt& node) {
    if (!isBoolean(typeOf(*node.condition))) {
        throw SemanticError("Loop condition must be bool (write x != 0 for an int).", node.loc);
    }

    node.body->accept(*this);
}

void SemanticAnalyzer::visit(ReturnStmt& node) {
    if (node.value) {
        if (currentReturnType->kind == TypeKind::Void) {
            throw SemanticError("A void function cannot return a value.", node.loc);
        }
        checkType(currentReturnType, typeOf(*node.value), node.loc);
    } else if (currentReturnType->kind != TypeKind::Void) {
        throw SemanticError("Return value required for non-void function.", node.loc);
    }
}

void SemanticAnalyzer::visit(DeleteStmt& node) {
    auto id = dynamic_cast<IdentifierExpr*>(node.pointer.get());
    if (!id) {
        throw SemanticError("delete expects a pointer variable, e.g. delete p;", node.loc);
    }
    if (!isPointer(typeOf(*node.pointer))) {
        throw SemanticError("Can only delete pointers.", node.loc);
    }
}

void SemanticAnalyzer::declareFunction(FunctionDecl& node) {
    auto symbol = std::make_shared<Symbol>();
    symbol->name = node.name;
    symbol->kind = SymbolKind::Function;
    symbol->type = convertTypeAST(node.returnType, node.loc);

    for (const auto& param : node.params) {
        auto paramType = convertTypeAST(param.type, node.loc);
        if (paramType->kind == TypeKind::Void) {
            throw SemanticError("Parameter '" + param.name + "' cannot have type void.", node.loc);
        }
        symbol->paramTypes.push_back(paramType);
    }

    if (!symTable.define(symbol)) {
        throw SemanticError("Function already declared: " + node.name, node.loc);
    }
    functions[node.name] = symbol;
}

void SemanticAnalyzer::visit(FunctionDecl& node) {
    auto symbol = functions.at(node.name);

    symTable.enterScope();
    currentReturnType = symbol->type;

    for (size_t i = 0; i < node.params.size(); ++i) {
        auto paramSymbol = std::make_shared<Symbol>();
        paramSymbol->name = node.params[i].name;
        paramSymbol->kind = SymbolKind::Variable;
        paramSymbol->type = symbol->paramTypes[i];
        if (!symTable.define(paramSymbol)) {
            throw SemanticError("Duplicate parameter name: " + node.params[i].name, node.loc);
        }
    }

    // Params and top-level body statements share one scope, as in C++
    for (auto& stmt : node.body->statements) {
        stmt->accept(*this);
    }

    // Falling off the end of a non-void function is undefined behaviour in C++; reject it.
    // main is exempt because C++ defines it to return 0.
    if (symbol->type->kind != TypeKind::Void && node.name != "main" && !alwaysReturns(*node.body)) {
        throw SemanticError("Function '" + node.name + "' can reach its end without returning a value.", node.loc);
    }

    symTable.leaveScope();
}

void SemanticAnalyzer::visit(Program& node) {
    // Pass 1: declare every function so calls may appear before definitions (and recursion works)
    for (auto& decl : node.declarations) {
        if (auto func = dynamic_cast<FunctionDecl*>(decl.get())) {
            declareFunction(*func);
        }
    }

    auto mainIt = functions.find("main");
    if (mainIt == functions.end()) {
        throw SemanticError("Program has no main function.", node.loc);
    }
    auto mainKind = mainIt->second->type->kind;
    if (!mainIt->second->paramTypes.empty() || (mainKind != TypeKind::Int && mainKind != TypeKind::Void)) {
        throw SemanticError("main must be declared as 'int main()' or 'void main()'.", node.loc);
    }

    // Pass 2: check bodies
    for (auto& decl : node.declarations) {
        decl->accept(*this);
    }
}

} // namespace jscpp
