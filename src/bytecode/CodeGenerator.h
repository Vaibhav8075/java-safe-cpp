#pragma once

#include "parser/AST.h"
#include "bytecode/Bytecode.h"
#include "semantic/SemanticAnalyzer.h"
#include <unordered_map>
#include <vector>

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
    struct LocalVar {
        std::string name;
        int slot;
        std::shared_ptr<Type> type;
    };

    SemanticAnalyzer& semantic;
    BytecodeProgram prog;

    // Local-variable slots of the function being generated; params take slots 0..n-1
    std::vector<std::vector<LocalVar>> scopes;
    int nextSlot = 0;
    std::shared_ptr<Type> currentReturnType;
    std::unordered_map<std::string, int> functionIndex;
    int currentLine = 0;

    int getInstructionOffset() const;
    void emit(Opcode op, int operand = 0);
    void emitConst(int value);
    void patchOperand(int instructionOffset, int newOperand);

    const LocalVar& resolve(const std::string& name) const;
    int defineLocal(const std::string& name, std::shared_ptr<Type> type);

    // ARC helpers
    static bool isManaged(const std::shared_ptr<Type>& type);
    bool isPointerExpr(Expr& expr);
    bool isOwnedTemp(Expr& expr);
    void emitRelease(const std::vector<LocalVar>& scope, bool clearSlots);
    void emitAssign(BinaryExpr& node);
};

} // namespace jscpp
