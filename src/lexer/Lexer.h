#pragma once

#include "Token.h"
#include <vector>
#include <string>

namespace jscpp {

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename = "<memory>");

    std::vector<Token> tokenize();

private:
    std::string source;
    std::string filename;
    size_t pos = 0;
    int line = 1;
    int column = 1;

    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;

    void skipWhitespaceAndComments();
    Token nextToken();
    Token identifierOrKeyword();
    Token number();

    SourceLocation currentLocation() const;
};

} // namespace jscpp
