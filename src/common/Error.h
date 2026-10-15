#pragma once

#include "SourceLocation.h"
#include <string>
#include <stdexcept>

namespace jscpp {

class CompilerError : public std::runtime_error {
    SourceLocation loc;
public:
    CompilerError(const std::string& msg, const SourceLocation& loc)
        : std::runtime_error(msg), loc(loc) {}

    const SourceLocation& getLocation() const { return loc; }
};

class LexerError : public CompilerError {
public:
    using CompilerError::CompilerError;
};

class ParserError : public CompilerError {
public:
    using CompilerError::CompilerError;
};

class SemanticError : public CompilerError {
public:
    using CompilerError::CompilerError;
};

} // namespace jscpp
