#pragma once

#include "AST.h"
#include <ostream>

namespace jscpp {

// Prints the parse tree as an indented outline (used by "jscpp ast")
class ASTPrinter : public ASTVisitor {
public:
    explicit ASTPrinter(std::ostream& os) : os(os) {}

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
    std::ostream& os;
    int depth = 0;

    void line(const std::string& text, const ASTNode& node);
    void child(ASTNode& node);
    void labelled(const std::string& label, ASTNode& node);
    static std::string typeName(const TypeAST& type);
};

} // namespace jscpp
