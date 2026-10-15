#include "Parser.h"
#include "common/Error.h"

namespace jscpp {

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

bool Parser::isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }

const Token& Parser::peek() const { return tokens[current]; }

const Token& Parser::previous() const { return tokens[current - 1]; }

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (check(type)) {
            current++;
            return true;
        }
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        current++;
        return previous();
    }
    throw ParserError(message, peek().loc);
}

std::unique_ptr<Program> Parser::parse() {
    std::vector<std::unique_ptr<ASTNode>> declarations;
    SourceLocation startLoc = peek().loc;
    while (!isAtEnd()) {
        declarations.push_back(declaration());
    }
    return std::make_unique<Program>(std::move(declarations), startLoc);
}

bool Parser::isTypeStart() const {
    return check(TokenType::KW_INT) || check(TokenType::KW_BOOL) || check(TokenType::KW_VOID);
}

TypeAST Parser::parseType() {
    if (!isTypeStart()) {
        throw ParserError("Expected a type (int, bool, void).", peek().loc);
    }
    std::string baseName = peek().lexeme;
    current++; // consume base type

    TypeAST type(baseName);

    if (match({TokenType::STAR})) {
        type.isPointer = true;
        if (check(TokenType::STAR)) {
            throw ParserError("Pointers to pointers are not supported in this subset.", peek().loc);
        }
    }

    return type;
}

std::unique_ptr<ASTNode> Parser::declaration() {
    TypeAST type = parseType();
    const Token& nameToken = consume(TokenType::IDENTIFIER, "Expected identifier after type.");

    if (match({TokenType::LPAREN})) {
        return functionDeclaration(type, nameToken.lexeme);
    }
    // The VM only has per-frame locals, so a global would silently read the wrong slot
    throw ParserError("Global variables are not supported; declare '" + nameToken.lexeme +
                      "' inside a function.", nameToken.loc);
}

std::unique_ptr<FunctionDecl> Parser::functionDeclaration(TypeAST returnType, const std::string& name) {
    SourceLocation loc = previous().loc;
    std::vector<Param> params;

    if (!check(TokenType::RPAREN)) {
        do {
            TypeAST paramType = parseType();
            const Token& paramName = consume(TokenType::IDENTIFIER, "Expected parameter name.");
            params.push_back({paramType, paramName.lexeme});
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters.");

    consume(TokenType::LBRACE, "Expected '{' before function body.");
    auto body = block();

    return std::make_unique<FunctionDecl>(returnType, name, std::move(params), std::move(body), loc);
}

std::unique_ptr<Stmt> Parser::varDeclaration(TypeAST type, const std::string& name) {
    SourceLocation loc = previous().loc;

    if (match({TokenType::LBRACKET})) {
        const Token& sizeToken = consume(TokenType::INT_LITERAL, "Expected a constant array size, e.g. int a[10];");
        consume(TokenType::RBRACKET, "Expected ']' after array size.");
        type.isArray = true;
        type.arraySize = std::stoi(sizeToken.lexeme);
        if (type.arraySize <= 0) {
            throw ParserError("Array size must be at least 1.", sizeToken.loc);
        }

        consume(TokenType::SEMICOLON, "Expected ';' after array declaration.");
        return std::make_unique<VarDeclStmt>(type, name, nullptr, loc);
    }

    std::unique_ptr<Expr> initializer = nullptr;
    if (match({TokenType::ASSIGN})) {
        initializer = expression();
    }

    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration.");
    return std::make_unique<VarDeclStmt>(type, name, std::move(initializer), loc);
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match({TokenType::KW_IF})) return ifStatement();
    if (match({TokenType::KW_WHILE})) return whileStatement();
    if (match({TokenType::KW_FOR})) return forStatement();
    if (match({TokenType::KW_RETURN})) return returnStatement();
    if (match({TokenType::KW_DELETE})) return deleteStatement();
    if (match({TokenType::LBRACE})) return block();

    if (isTypeStart()) {
        TypeAST type = parseType();
        const Token& nameToken = consume(TokenType::IDENTIFIER, "Expected identifier after type.");
        return varDeclaration(type, nameToken.lexeme);
    }

    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::ifStatement() {
    SourceLocation loc = previous().loc;
    consume(TokenType::LPAREN, "Expected '(' after 'if'.");
    auto condition = expression();
    consume(TokenType::RPAREN, "Expected ')' after if condition.");

    auto thenBranch = statement();
    std::unique_ptr<Stmt> elseBranch = nullptr;

    if (match({TokenType::KW_ELSE})) {
        elseBranch = statement();
    }

    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch), loc);
}

std::unique_ptr<Stmt> Parser::whileStatement() {
    SourceLocation loc = previous().loc;
    consume(TokenType::LPAREN, "Expected '(' after 'while'.");
    auto condition = expression();
    consume(TokenType::RPAREN, "Expected ')' after while condition.");
    auto body = statement();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), loc);
}

std::unique_ptr<Stmt> Parser::forStatement() {
    SourceLocation loc = previous().loc;
    consume(TokenType::LPAREN, "Expected '(' after 'for'.");

    std::unique_ptr<Stmt> initializer;
    if (match({TokenType::SEMICOLON})) {
        initializer = nullptr;
    } else if (isTypeStart()) {
        TypeAST type = parseType();
        const Token& nameToken = consume(TokenType::IDENTIFIER, "Expected identifier after type.");
        initializer = varDeclaration(type, nameToken.lexeme);
    } else {
        initializer = expressionStatement();
    }

    std::unique_ptr<Expr> condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after loop condition.");

    std::unique_ptr<Expr> increment = nullptr;
    if (!check(TokenType::RPAREN)) {
        increment = expression();
    }
    consume(TokenType::RPAREN, "Expected ')' after for clauses.");

    auto body = statement();

    // Desugar for loop into while loop
    if (increment != nullptr) {
        std::vector<std::unique_ptr<Stmt>> blockStmts;
        blockStmts.push_back(std::move(body));
        blockStmts.push_back(std::make_unique<ExprStmt>(std::move(increment), loc));
        body = std::make_unique<BlockStmt>(std::move(blockStmts), loc);
    }

    if (condition == nullptr) {
        condition = std::make_unique<BoolLiteral>(true, loc);
    }

    body = std::make_unique<WhileStmt>(std::move(condition), std::move(body), loc);

    if (initializer != nullptr) {
        std::vector<std::unique_ptr<Stmt>> blockStmts;
        blockStmts.push_back(std::move(initializer));
        blockStmts.push_back(std::move(body));
        body = std::make_unique<BlockStmt>(std::move(blockStmts), loc);
    }

    return body;
}

std::unique_ptr<BlockStmt> Parser::block() {
    SourceLocation loc = previous().loc;
    std::vector<std::unique_ptr<Stmt>> statements;

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(statement());
    }

    consume(TokenType::RBRACE, "Expected '}' after block.");
    return std::make_unique<BlockStmt>(std::move(statements), loc);
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    SourceLocation loc = previous().loc;
    std::unique_ptr<Expr> value = nullptr;

    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }

    consume(TokenType::SEMICOLON, "Expected ';' after return value.");
    return std::make_unique<ReturnStmt>(std::move(value), loc);
}

std::unique_ptr<Stmt> Parser::deleteStatement() {
    SourceLocation loc = previous().loc;
    auto pointer = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after delete.");
    return std::make_unique<DeleteStmt>(std::move(pointer), loc);
}

std::unique_ptr<Stmt> Parser::expressionStatement() {
    auto expr = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression.");
    SourceLocation loc = expr->loc;
    return std::make_unique<ExprStmt>(std::move(expr), loc);
}

std::unique_ptr<Expr> Parser::expression() {
    return assignment();
}

std::unique_ptr<Expr> Parser::assignment() {
    auto expr = logicalOr();

    if (match({TokenType::ASSIGN})) {
        Token equals = previous();
        auto value = assignment();
        // Whether the left side is a valid l-value is checked by the semantic analyzer
        return std::make_unique<BinaryExpr>(std::move(expr), equals, std::move(value), equals.loc);
    }

    if (match({TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN, TokenType::STAR_ASSIGN,
               TokenType::SLASH_ASSIGN, TokenType::PERCENT_ASSIGN})) {
        Token op = previous();
        auto value = assignment();
        TokenType arith = TokenType::PLUS;
        switch (op.type) {
            case TokenType::MINUS_ASSIGN: arith = TokenType::MINUS; break;
            case TokenType::STAR_ASSIGN: arith = TokenType::STAR; break;
            case TokenType::SLASH_ASSIGN: arith = TokenType::SLASH; break;
            case TokenType::PERCENT_ASSIGN: arith = TokenType::PERCENT; break;
            default: break;
        }
        return makeUpdate(std::move(expr), arith, std::move(value), op);
    }

    return expr;
}

// Copies an l-value expression that has no side effects (x, a[i], a[3], *p) so that
// "x += y" can be rewritten as "x = x + y" without evaluating anything twice.
std::unique_ptr<Expr> Parser::cloneSimple(const Expr& expr) {
    if (auto id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        return std::make_unique<IdentifierExpr>(id->name, id->loc);
    }
    if (auto lit = dynamic_cast<const IntLiteral*>(&expr)) {
        return std::make_unique<IntLiteral>(lit->value, lit->loc);
    }
    if (auto arr = dynamic_cast<const ArrayAccessExpr*>(&expr)) {
        auto index = cloneSimple(*arr->index);
        if (!index) return nullptr;
        return std::make_unique<ArrayAccessExpr>(arr->arrayName, std::move(index), arr->loc);
    }
    if (auto deref = dynamic_cast<const DerefExpr*>(&expr)) {
        if (!dynamic_cast<const IdentifierExpr*>(deref->expr.get())) return nullptr;
        return std::make_unique<DerefExpr>(cloneSimple(*deref->expr), deref->loc);
    }
    return nullptr;
}

std::unique_ptr<Expr> Parser::makeUpdate(std::unique_ptr<Expr> target, TokenType arithOp,
                                         std::unique_ptr<Expr> amount, const Token& opToken) {
    auto copy = cloneSimple(*target);
    if (!copy || dynamic_cast<IntLiteral*>(target.get())) {
        throw ParserError("'" + opToken.lexeme + "' needs a variable, an array element with a simple index, "
                          "or *pointer on its left side.", opToken.loc);
    }
    std::string arithLexeme = arithOp == TokenType::PLUS ? "+" : arithOp == TokenType::MINUS ? "-" :
                              arithOp == TokenType::STAR ? "*" : arithOp == TokenType::SLASH ? "/" : "%";
    Token arith(arithOp, arithLexeme, opToken.loc);
    Token assign(TokenType::ASSIGN, "=", opToken.loc);
    auto combined = std::make_unique<BinaryExpr>(std::move(copy), arith, std::move(amount), opToken.loc);
    return std::make_unique<BinaryExpr>(std::move(target), assign, std::move(combined), opToken.loc);
}

std::unique_ptr<Expr> Parser::logicalOr() {
    auto expr = logicalAnd();
    while (match({TokenType::OR})) {
        Token op = previous();
        auto right = logicalAnd();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::logicalAnd() {
    auto expr = equality();
    while (match({TokenType::AND})) {
        Token op = previous();
        auto right = equality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    auto expr = comparison();
    while (match({TokenType::EQ, TokenType::NEQ})) {
        Token op = previous();
        auto right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    auto expr = term();
    while (match({TokenType::LT, TokenType::LTE, TokenType::GT, TokenType::GTE})) {
        Token op = previous();
        auto right = term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    auto expr = factor();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        auto right = factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    auto expr = unary();
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        auto right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (match({TokenType::INC, TokenType::DEC})) {
        // ++x  ->  x = x + 1   (the expression's value is the new value)
        Token op = previous();
        auto target = unary();
        auto one = std::make_unique<IntLiteral>(1, op.loc);
        return makeUpdate(std::move(target), op.type == TokenType::INC ? TokenType::PLUS : TokenType::MINUS,
                          std::move(one), op);
    }
    if (match({TokenType::NOT, TokenType::MINUS, TokenType::STAR})) {
        Token op = previous();
        auto right = unary();

        if (op.type == TokenType::STAR) {
            return std::make_unique<DerefExpr>(std::move(right), op.loc);
        }

        return std::make_unique<UnaryExpr>(op, std::move(right), op.loc);
    }
    return postfix();
}

std::unique_ptr<Expr> Parser::postfix() {
    auto expr = primary();

    while (true) {
        if (match({TokenType::LBRACKET})) {
            Token bracket = previous();
            auto base = dynamic_cast<IdentifierExpr*>(expr.get());
            if (!base) {
                throw ParserError("Only a named array can be indexed, e.g. arr[i].", bracket.loc);
            }
            auto index = expression();
            consume(TokenType::RBRACKET, "Expected ']' after array index.");
            expr = std::make_unique<ArrayAccessExpr>(base->name, std::move(index), bracket.loc);
        } else if (match({TokenType::INC, TokenType::DEC})) {
            // x++  ->  (x = x + 1) - 1   (the expression's value is the old value)
            Token op = previous();
            bool inc = op.type == TokenType::INC;
            auto one = std::make_unique<IntLiteral>(1, op.loc);
            auto updated = makeUpdate(std::move(expr), inc ? TokenType::PLUS : TokenType::MINUS, std::move(one), op);
            Token undo(inc ? TokenType::MINUS : TokenType::PLUS, inc ? "-" : "+", op.loc);
            expr = std::make_unique<BinaryExpr>(std::move(updated), undo,
                                                std::make_unique<IntLiteral>(1, op.loc), op.loc);
        } else if (match({TokenType::LPAREN})) {
            Token paren = previous();
            if (!dynamic_cast<IdentifierExpr*>(expr.get())) {
                throw ParserError("Only a named function can be called.", paren.loc);
            }
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(expression());
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments.");
            std::string calleeName = dynamic_cast<IdentifierExpr*>(expr.get()) ? dynamic_cast<IdentifierExpr*>(expr.get())->name : "";
            expr = std::make_unique<CallExpr>(calleeName, std::move(args), paren.loc);
        } else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Expr> Parser::primary() {
    if (match({TokenType::KW_FALSE})) return std::make_unique<BoolLiteral>(false, previous().loc);
    if (match({TokenType::KW_TRUE})) return std::make_unique<BoolLiteral>(true, previous().loc);
    if (match({TokenType::KW_NULLPTR})) return std::make_unique<NullptrLiteral>(previous().loc);

    if (match({TokenType::INT_LITERAL})) {
        return std::make_unique<IntLiteral>(std::stoi(previous().lexeme), previous().loc);
    }

    if (match({TokenType::IDENTIFIER})) {
        return std::make_unique<IdentifierExpr>(previous().lexeme, previous().loc);
    }

    if (match({TokenType::KW_NEW})) {
        SourceLocation loc = previous().loc;
        TypeAST type = parseType();
        if (type.isPointer || check(TokenType::LBRACKET)) {
            throw ParserError("Only 'new int' and 'new bool' are supported (no new T* or new T[n]).", loc);
        }
        return std::make_unique<NewExpr>(Token(TokenType::IDENTIFIER, type.name, loc), loc);
    }

    if (match({TokenType::LPAREN})) {
        auto expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression.");
        return expr;
    }

    throw ParserError("Expected expression.", peek().loc);
}

} // namespace jscpp
