#pragma once

#include "parser/AST.h"
#include "bytecode/Bytecode.h"
#include "semantic/SymbolTable.h"
#include "semantic/SemanticAnalyzer.h"

namespace jscpp {

class CodeGenerator : public ASTVisitor {
public:
    CodeGenerator(SemanticAnalyzer& semantic);
    BytecodeProgram generate(Program& program);

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
    SemanticAnalyzer& semantic;
    BytecodeProgram prog;
    SymbolTable symTable; // Need this for local variable indexing during codegen

    int getInstructionOffset() const;
    void emit(Opcode op, int operand = 0);
    void patchOperand(int instructionOffset, int newOperand);

    bool isLValue = false; // Flag to indicate if we're evaluating an l-value
};

} // namespace jscpp
