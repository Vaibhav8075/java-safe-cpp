#pragma once

#include "AST.h"
#include "lexer/Token.h"
#include <vector>

namespace jscpp {

class Parser {
public:
    Parser(std::vector<Token> tokens);
    std::unique_ptr<Program> parse();

private:
    std::vector<Token> tokens;
    size_t current = 0;

    // Helper methods
    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    bool check(TokenType type) const;
    bool match(std::initializer_list<TokenType> types);
    const Token& consume(TokenType type, const std::string& message);

    bool isTypeStart() const;

    // Parsing methods
    std::unique_ptr<ASTNode> declaration();
    std::unique_ptr<FunctionDecl> functionDeclaration(TypeAST returnType, const std::string& name);
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> varDeclaration(TypeAST type, const std::string& name);

    std::unique_ptr<Stmt> ifStatement();
    std::unique_ptr<Stmt> whileStatement();
    std::unique_ptr<Stmt> forStatement();
    std::unique_ptr<BlockStmt> block();
    std::unique_ptr<Stmt> returnStatement();
    std::unique_ptr<Stmt> deleteStatement();
    std::unique_ptr<Stmt> expressionStatement();

    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> assignment();
    std::unique_ptr<Expr> logicalOr();
    std::unique_ptr<Expr> logicalAnd();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> postfix();
    std::unique_ptr<Expr> primary();

    TypeAST parseType();
};

} // namespace jscpp
