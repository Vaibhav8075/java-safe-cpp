#include "Lexer.h"
#include "common/Error.h"
#include <cctype>
#include <climits>
#include <unordered_map>
#include <unordered_set>

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

// Keywords of full C++ that this subset deliberately does not support.
static const std::unordered_set<std::string> excludedFeatures = {
    "template", "typename", "class", "struct", "union", "public", "private", "protected",
    "virtual", "namespace", "using", "operator", "friend", "goto", "throw", "try", "catch"
};

static const std::unordered_set<std::string> excludedTypes = {
    "char", "float", "double", "long", "short", "unsigned", "signed", "auto", "string"
};

// <cctype> functions are undefined for negative char values, so always widen to unsigned char first
static bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

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
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            SourceLocation start = currentLocation();
            advance(); // '/'
            advance(); // '*'
            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                throw LexerError("Unterminated block comment.", start);
            }
        } else {
            break;
        }
    }
}

Token Lexer::nextToken() {
    SourceLocation loc = currentLocation();
    char first = peek();
    if (isIdentStart(first)) return identifierOrKeyword();
    if (isDigit(first)) return number();

    char c = advance();
    switch (c) {
        case '+':
            if (match('+')) return Token(TokenType::INC, "++", loc);
            if (match('=')) return Token(TokenType::PLUS_ASSIGN, "+=", loc);
            return Token(TokenType::PLUS, "+", loc);
        case '-':
            if (match('-')) return Token(TokenType::DEC, "--", loc);
            if (match('=')) return Token(TokenType::MINUS_ASSIGN, "-=", loc);
            return Token(TokenType::MINUS, "-", loc);
        case '*': return match('=') ? Token(TokenType::STAR_ASSIGN, "*=", loc) : Token(TokenType::STAR, "*", loc);
        case '/': return match('=') ? Token(TokenType::SLASH_ASSIGN, "/=", loc) : Token(TokenType::SLASH, "/", loc);
        case '%': return match('=') ? Token(TokenType::PERCENT_ASSIGN, "%=", loc) : Token(TokenType::PERCENT, "%", loc);
        case '=': return match('=') ? Token(TokenType::EQ, "==", loc) : Token(TokenType::ASSIGN, "=", loc);
        case '!': return match('=') ? Token(TokenType::NEQ, "!=", loc) : Token(TokenType::NOT, "!", loc);
        case '<': return match('=') ? Token(TokenType::LTE, "<=", loc) : Token(TokenType::LT, "<", loc);
        case '>': return match('=') ? Token(TokenType::GTE, ">=", loc) : Token(TokenType::GT, ">", loc);
        case '&':
            if (match('&')) return Token(TokenType::AND, "&&", loc);
            throw LexerError("The address-of operator '&' and references are not supported; use 'new' to create pointers.", loc);
        case '|':
            if (match('|')) return Token(TokenType::OR, "||", loc);
            throw LexerError("Bitwise '|' is not supported; did you mean '||'?", loc);
        case '(': return Token(TokenType::LPAREN, "(", loc);
        case ')': return Token(TokenType::RPAREN, ")", loc);
        case '{': return Token(TokenType::LBRACE, "{", loc);
        case '}': return Token(TokenType::RBRACE, "}", loc);
        case '[': return Token(TokenType::LBRACKET, "[", loc);
        case ']': return Token(TokenType::RBRACKET, "]", loc);
        case ';': return Token(TokenType::SEMICOLON, ";", loc);
        case ',': return Token(TokenType::COMMA, ",", loc);
        case '#':
            throw LexerError("Preprocessor directives (#include, #define) and the STL are not part of this C++ subset.", loc);
        case '"':
        case '\'':
            throw LexerError("String and character literals are not supported; only int and bool values exist.", loc);
        default: {
            unsigned char u = static_cast<unsigned char>(c);
            if (u < 32 || u >= 127) {
                static const char* hex = "0123456789ABCDEF";
                std::string code = {hex[u >> 4], hex[u & 15]};
                throw LexerError("Unexpected byte 0x" + code + " (source files must be plain ASCII/UTF-8 text).", loc);
            }
            throw LexerError(std::string("Unexpected character '") + c + "'", loc);
        }
    }
}

Token Lexer::identifierOrKeyword() {
    SourceLocation loc = currentLocation();
    std::string lexeme;
    while (!isAtEnd() && isIdentChar(peek())) {
        lexeme += advance();
    }

    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        return Token(it->second, lexeme, loc);
    }

    // Reject full-C++ features up front with a clear message instead of a confusing parse error
    if (excludedFeatures.count(lexeme)) {
        throw LexerError("'" + lexeme + "' is not supported: templates, classes, structs, namespaces "
                         "and exceptions are excluded from this C++ subset.", loc);
    }
    if (excludedTypes.count(lexeme)) {
        throw LexerError("Type '" + lexeme + "' is not supported; the subset only has int, bool and void.", loc);
    }

    return Token(TokenType::IDENTIFIER, lexeme, loc);
}

Token Lexer::number() {
    SourceLocation loc = currentLocation();
    std::string lexeme;
    while (!isAtEnd() && isDigit(peek())) {
        lexeme += advance();
    }
    if (!isAtEnd() && isIdentStart(peek())) {
        throw LexerError("Invalid number literal starting with '" + lexeme + "'.", loc);
    }
    // Validate here so the parser's std::stoi can never throw out_of_range
    if (lexeme.size() > 10 || std::stoll(lexeme) > INT_MAX) {
        throw LexerError("Integer literal " + lexeme + " does not fit in a 32-bit int (max 2147483647).", loc);
    }
    return Token(TokenType::INT_LITERAL, lexeme, loc);
}

} // namespace jscpp
