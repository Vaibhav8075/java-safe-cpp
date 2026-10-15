#include "Lexer.h"
#include "common/Error.h"
#include <cctype>
#include <unordered_map>

namespace jscpp {

static const std::unordered_map<std::string, TokenType> keywords = {
    {"int", TokenType::KW_INT},
    {"bool", TokenType::KW_BOOL},
    {"void", TokenType::KW_VOID},
    {"if", TokenType::KW_IF},
    {"else", TokenType::KW_ELSE},
    {"while", TokenType::KW_WHILE},
    {"for", TokenType::KW_FOR},
    {"return", TokenType::KW_RETURN},
    {"new", TokenType::KW_NEW},
    {"delete", TokenType::KW_DELETE},
    {"true", TokenType::KW_TRUE},
    {"false", TokenType::KW_FALSE},
    {"nullptr", TokenType::KW_NULLPTR}
};

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source(source), filename(filename) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) break;
        tokens.push_back(nextToken());
    }
    tokens.emplace_back(TokenType::END_OF_FILE, "", currentLocation());
    return tokens;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[pos];
}

char Lexer::peekNext() const {
    if (pos + 1 >= source.length()) return '\0';
    return source[pos + 1];
}

char Lexer::advance() {
    char c = source[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[pos] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return pos >= source.length();
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation(filename, line, column);
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (std::isspace(c)) {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            advance(); // '/'
            advance(); // '*'
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::nextToken() {
    SourceLocation loc = currentLocation();
    char c = advance();

    if (std::isalpha(c) || c == '_') {
        pos--;
        if (c == '\n') { line--; column = 1; } else { column--; }
        return identifierOrKeyword();
    }
    if (std::isdigit(c)) {
        pos--;
        column--;
        return number();
    }

    switch (c) {
        case '+': return match('+') ? Token(TokenType::INC, "++", loc) : Token(TokenType::PLUS, "+", loc);
        case '-': return match('-') ? Token(TokenType::DEC, "--", loc) : Token(TokenType::MINUS, "-", loc);
        case '*': return Token(TokenType::STAR, "*", loc);
        case '/': return Token(TokenType::SLASH, "/", loc);
        case '%': return Token(TokenType::PERCENT, "%", loc);
        case '=': return match('=') ? Token(TokenType::EQ, "==", loc) : Token(TokenType::ASSIGN, "=", loc);
        case '!': return match('=') ? Token(TokenType::NEQ, "!=", loc) : Token(TokenType::NOT, "!", loc);
        case '<': return match('=') ? Token(TokenType::LTE, "<=", loc) : Token(TokenType::LT, "<", loc);
        case '>': return match('=') ? Token(TokenType::GTE, ">=", loc) : Token(TokenType::GT, ">", loc);
        case '&':
            if (match('&')) return Token(TokenType::AND, "&&", loc);
            throw LexerError("Unexpected character '&'", loc);
        case '|':
            if (match('|')) return Token(TokenType::OR, "||", loc);
            throw LexerError("Unexpected character '|'", loc);
        case '(': return Token(TokenType::LPAREN, "(", loc);
        case ')': return Token(TokenType::RPAREN, ")", loc);
        case '{': return Token(TokenType::LBRACE, "{", loc);
        case '}': return Token(TokenType::RBRACE, "}", loc);
        case '[': return Token(TokenType::LBRACKET, "[", loc);
        case ']': return Token(TokenType::RBRACKET, "]", loc);
        case ';': return Token(TokenType::SEMICOLON, ";", loc);
        case ',': return Token(TokenType::COMMA, ",", loc);
        case '#':
            throw LexerError("Error: Preprocessor macros (like #include or #define) and STL are explicitly excluded in this minimal C++ subset.", loc);
        default:
            throw LexerError(std::string("Unexpected character '") + c + "'", loc);
    }
}

Token Lexer::identifierOrKeyword() {
    SourceLocation loc = currentLocation();
    std::string lexeme;
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
        lexeme += advance();
    }

    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        return Token(it->second, lexeme, loc);
    }

    // Explicitly reject unsupported C++ features as requested by feedback
    if (lexeme == "template" || lexeme == "class" || lexeme == "struct" ||
        lexeme == "public" || lexeme == "private" || lexeme == "virtual") {
        throw LexerError("Error: This project only supports a minimal subset of C++. Features like templates, classes, and structs are explicitly excluded.", loc);
    }

    return Token(TokenType::IDENTIFIER, lexeme, loc);
}

Token Lexer::number() {
    SourceLocation loc = currentLocation();
    std::string lexeme;
    while (!isAtEnd() && std::isdigit(peek())) {
        lexeme += advance();
    }
    return Token(TokenType::INT_LITERAL, lexeme, loc);
}

} // namespace jscpp
