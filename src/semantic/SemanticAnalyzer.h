#pragma once

#include "parser/AST.h"
#include "semantic/SymbolTable.h"
#include "semantic/Type.h"
#include <unordered_map>
#include <memory>

namespace jscpp {

class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer();

    void analyze(Program& program);

    // Type of every expression, plus the declared type of every VarDeclStmt
    std::unordered_map<ASTNode*, std::shared_ptr<Type>> nodeTypes;

    // Signature of every function (including the built-in print), by name
    std::unordered_map<std::string, std::shared_ptr<Symbol>> functions;

    // Visit methods
    void visit(IntLiteral& node) override;
    void visit(BoolLiteral& node) override;
    void visit(NullptrLiteral& node) override;
    void visit(IdentifierExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(ArrayAccessExpr& node) override;
    void visit(NewExpr& node) override;
    void visit(DerefExpr& node) override;

    void visit(BlockStmt& node) override;
    void visit(ExprStmt& node) override;
    void visit(VarDeclStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(DeleteStmt& node) override;

    void visit(FunctionDecl& node) override;
    void visit(Program& node) override;

private:
    SymbolTable symTable;
    std::shared_ptr<Type> currentReturnType;

    std::shared_ptr<Type> convertTypeAST(const TypeAST& typeAST, const SourceLocation& loc);
    void declareFunction(FunctionDecl& node);
    std::shared_ptr<Type> typeOf(Expr& expr);
    void checkType(const std::shared_ptr<Type>& expected, const std::shared_ptr<Type>& actual, const SourceLocation& loc);
    void checkAssignable(Expr& target);
    static bool alwaysReturns(const Stmt& stmt);
    bool isNumeric(const std::shared_ptr<Type>& type);
    bool isBoolean(const std::shared_ptr<Type>& type);
    bool isPointer(const std::shared_ptr<Type>& type);
    bool isArray(const std::shared_ptr<Type>& type);
};

} // namespace jscpp
