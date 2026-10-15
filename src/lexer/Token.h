#pragma once

#include <string>
#include <string_view>
#include "common/SourceLocation.h"

namespace jscpp {

enum class TokenType {
    // Keywords
    KW_INT, KW_BOOL, KW_VOID,
    KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_RETURN,
    KW_NEW, KW_DELETE, KW_TRUE, KW_FALSE, KW_NULLPTR,

    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    ASSIGN, EQ, NEQ, LT, GT, LTE, GTE,
    AND, OR, NOT, INC, DEC,

    // Punctuation
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COMMA,

    // Literals and Identifiers
    IDENTIFIER,
    INT_LITERAL,

    // Special
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
    SourceLocation loc;

    Token(TokenType t, std::string lex, SourceLocation l)
        : type(t), lexeme(std::move(lex)), loc(std::move(l)) {}

    std::string toString() const;
    static std::string typeToString(TokenType type);
};

} // namespace jscpp
