#pragma once

#include <stdexcept>
#include <string>

namespace jscpp {

class RuntimeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class NullPointerException : public RuntimeError {
public:
    NullPointerException(const std::string& msg = "NullPointerException: attempted to dereference a null pointer")
        : RuntimeError(msg) {}
};

class ArrayIndexOutOfBoundsException : public RuntimeError {
public:
    ArrayIndexOutOfBoundsException(int index, int length)
        : RuntimeError("ArrayIndexOutOfBoundsException: index " + std::to_string(index) + ", length " + std::to_string(length)) {}
};

class DivisionByZeroException : public RuntimeError {
public:
    DivisionByZeroException() : RuntimeError("DivisionByZeroException: division by zero") {}
};

class InvalidInstructionException : public RuntimeError {
public:
    InvalidInstructionException(const std::string& msg = "InvalidInstructionException")
        : RuntimeError(msg) {}
};

class StackOverflowException : public RuntimeError {
public:
    StackOverflowException() : RuntimeError("StackOverflowException: call stack exceeded limit") {}
};

} // namespace jscpp
