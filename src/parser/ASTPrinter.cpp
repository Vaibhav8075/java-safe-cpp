#include "ASTPrinter.h"

namespace jscpp {

void ASTPrinter::line(const std::string& text, const ASTNode& node) {
    os << std::string(depth * 2, ' ') << text << "  <line " << node.loc.line << ">\n";
}

void ASTPrinter::child(ASTNode& node) {
    depth++;
    node.accept(*this);
    depth--;
}

void ASTPrinter::labelled(const std::string& label, ASTNode& node) {
    depth++;
    os << std::string(depth * 2, ' ') << label << ":\n";
    child(node);
    depth--;
}

std::string ASTPrinter::typeName(const TypeAST& type) {
    std::string name = type.name;
    if (type.isPointer) name += "*";
    if (type.isArray) name += "[" + std::to_string(type.arraySize) + "]";
    return name;
}

void ASTPrinter::visit(IntLiteral& node) { line("Int " + std::to_string(node.value), node); }
void ASTPrinter::visit(BoolLiteral& node) { line(std::string("Bool ") + (node.value ? "true" : "false"), node); }
void ASTPrinter::visit(NullptrLiteral& node) { line("Nullptr", node); }
void ASTPrinter::visit(IdentifierExpr& node) { line("Var " + node.name, node); }

void ASTPrinter::visit(BinaryExpr& node) {
    line(node.op.type == TokenType::ASSIGN ? "Assign" : "Binary " + node.op.lexeme, node);
    child(*node.left);
    child(*node.right);
}

void ASTPrinter::visit(UnaryExpr& node) {
    line("Unary " + node.op.lexeme, node);
    child(*node.expr);
}

void ASTPrinter::visit(CallExpr& node) {
    line("Call " + node.callee, node);
    for (auto& arg : node.args) child(*arg);
}

void ASTPrinter::visit(ArrayAccessExpr& node) {
    line("Index " + node.arrayName, node);
    child(*node.index);
}

void ASTPrinter::visit(NewExpr& node) { line("New " + node.typeName.lexeme, node); }

void ASTPrinter::visit(DerefExpr& node) {
    line("Deref", node);
    child(*node.expr);
}

void ASTPrinter::visit(BlockStmt& node) {
    line("Block", node);
    for (auto& stmt : node.statements) child(*stmt);
}

void ASTPrinter::visit(ExprStmt& node) {
    line("ExprStmt", node);
    child(*node.expr);
}

void ASTPrinter::visit(VarDeclStmt& node) {
    line("VarDecl " + typeName(node.type) + " " + node.name, node);
    if (node.initializer) child(*node.initializer);
}

void ASTPrinter::visit(IfStmt& node) {
    line("If", node);
    labelled("cond", *node.condition);
    labelled("then", *node.thenBranch);
    if (node.elseBranch) labelled("else", *node.elseBranch);
}

void ASTPrinter::visit(WhileStmt& node) {
    line("While", node);
    labelled("cond", *node.condition);
    labelled("body", *node.body);
}

void ASTPrinter::visit(ReturnStmt& node) {
    line("Return", node);
    if (node.value) child(*node.value);
}

void ASTPrinter::visit(DeleteStmt& node) {
    line("Delete", node);
    child(*node.pointer);
}

void ASTPrinter::visit(FunctionDecl& node) {
    std::string sig = "Function " + typeName(node.returnType) + " " + node.name + "(";
    for (size_t i = 0; i < node.params.size(); ++i) {
        sig += (i ? ", " : "") + typeName(node.params[i].type) + " " + node.params[i].name;
    }
    line(sig + ")", node);
    child(*node.body);
}

void ASTPrinter::visit(Program& node) {
    line("Program", node);
    for (auto& decl : node.declarations) child(*decl);
}

} // namespace jscpp
