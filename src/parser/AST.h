#pragma once

#include "lexer/Token.h"
#include <memory>
#include <vector>
#include <string>

namespace jscpp {

class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;

    SourceLocation loc;
protected:
    ASTNode(SourceLocation loc) : loc(std::move(loc)) {}
};

class Expr : public ASTNode {
protected:
    Expr(SourceLocation loc) : ASTNode(std::move(loc)) {}
};

class Stmt : public ASTNode {
protected:
    Stmt(SourceLocation loc) : ASTNode(std::move(loc)) {}
};

// Expressions
class IntLiteral : public Expr {
public:
    int value;
    IntLiteral(int value, SourceLocation loc) : Expr(loc), value(value) {}
    void accept(ASTVisitor& visitor) override;
};

class BoolLiteral : public Expr {
public:
    bool value;
    BoolLiteral(bool value, SourceLocation loc) : Expr(loc), value(value) {}
    void accept(ASTVisitor& visitor) override;
};

class NullptrLiteral : public Expr {
public:
    NullptrLiteral(SourceLocation loc) : Expr(loc) {}
    void accept(ASTVisitor& visitor) override;
};

class IdentifierExpr : public Expr {
public:
    std::string name;
    IdentifierExpr(std::string name, SourceLocation loc) : Expr(loc), name(std::move(name)) {}
    void accept(ASTVisitor& visitor) override;
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;

    BinaryExpr(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right, SourceLocation loc)
        : Expr(loc), left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    void accept(ASTVisitor& visitor) override;
};

class UnaryExpr : public Expr {
public:
    Token op;
    std::unique_ptr<Expr> expr;

    UnaryExpr(Token op, std::unique_ptr<Expr> expr, SourceLocation loc)
        : Expr(loc), op(std::move(op)), expr(std::move(expr)) {}
    void accept(ASTVisitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args, SourceLocation loc)
        : Expr(loc), callee(std::move(callee)), args(std::move(args)) {}
    void accept(ASTVisitor& visitor) override;
};

class ArrayAccessExpr : public Expr {
public:
    std::string arrayName;
    std::unique_ptr<Expr> index;

    ArrayAccessExpr(std::string arrayName, std::unique_ptr<Expr> index, SourceLocation loc)
        : Expr(loc), arrayName(std::move(arrayName)), index(std::move(index)) {}
    void accept(ASTVisitor& visitor) override;
};

class NewExpr : public Expr {
public:
    Token typeName;

    NewExpr(Token typeName, SourceLocation loc)
        : Expr(loc), typeName(std::move(typeName)) {}
    void accept(ASTVisitor& visitor) override;
};

class DerefExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;

    DerefExpr(std::unique_ptr<Expr> expr, SourceLocation loc)
        : Expr(loc), expr(std::move(expr)) {}
    void accept(ASTVisitor& visitor) override;
};

// Types representing type AST
class TypeAST {
public:
    std::string name;
    bool isPointer = false;
    bool isArray = false;
    int arraySize = 0; // If array

    TypeAST(std::string n) : name(std::move(n)) {}
};

// Statements
class BlockStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    BlockStmt(std::vector<std::unique_ptr<Stmt>> statements, SourceLocation loc)
        : Stmt(loc), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expr;
    ExprStmt(std::unique_ptr<Expr> expr, SourceLocation loc)
        : Stmt(loc), expr(std::move(expr)) {}
    void accept(ASTVisitor& visitor) override;
};

class VarDeclStmt : public Stmt {
public:
    TypeAST type;
    std::string name;
    std::unique_ptr<Expr> initializer; // optional

    VarDeclStmt(TypeAST type, std::string name, std::unique_ptr<Expr> initializer, SourceLocation loc)
        : Stmt(loc), type(std::move(type)), name(std::move(name)), initializer(std::move(initializer)) {}
    void accept(ASTVisitor& visitor) override;
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch; // optional

    IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch, SourceLocation loc)
        : Stmt(loc), condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
    void accept(ASTVisitor& visitor) override;
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body, SourceLocation loc)
        : Stmt(loc), condition(std::move(condition)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class ReturnStmt : public Stmt {
public:
    std::unique_ptr<Expr> value; // optional

    ReturnStmt(std::unique_ptr<Expr> value, SourceLocation loc)
        : Stmt(loc), value(std::move(value)) {}
    void accept(ASTVisitor& visitor) override;
};

class DeleteStmt : public Stmt {
public:
    std::unique_ptr<Expr> pointer;

    DeleteStmt(std::unique_ptr<Expr> pointer, SourceLocation loc)
        : Stmt(loc), pointer(std::move(pointer)) {}
    void accept(ASTVisitor& visitor) override;
};

// Function declaration
struct Param {
    TypeAST type;
    std::string name;
};

class FunctionDecl : public ASTNode {
public:
    TypeAST returnType;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;

    FunctionDecl(TypeAST returnType, std::string name, std::vector<Param> params, std::unique_ptr<BlockStmt> body, SourceLocation loc)
        : ASTNode(loc), returnType(std::move(returnType)), name(std::move(name)), params(std::move(params)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> declarations; // Functions or global variables

    Program(std::vector<std::unique_ptr<ASTNode>> declarations, SourceLocation loc)
        : ASTNode(loc), declarations(std::move(declarations)) {}
    void accept(ASTVisitor& visitor) override;
};

// Visitor Interface
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(IntLiteral& node) = 0;
    virtual void visit(BoolLiteral& node) = 0;
    virtual void visit(NullptrLiteral& node) = 0;
    virtual void visit(IdentifierExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;
    virtual void visit(ArrayAccessExpr& node) = 0;
    virtual void visit(NewExpr& node) = 0;
    virtual void visit(DerefExpr& node) = 0;

    virtual void visit(BlockStmt& node) = 0;
    virtual void visit(ExprStmt& node) = 0;
    virtual void visit(VarDeclStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(DeleteStmt& node) = 0;

    virtual void visit(FunctionDecl& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace jscpp
