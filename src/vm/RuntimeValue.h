#pragma once

namespace jscpp {

struct RuntimeValue {
    int value; // Can hold int, bool, or pointer (reference to Heap object)

    RuntimeValue() : value(0) {}
    RuntimeValue(int v) : value(v) {}
    RuntimeValue(bool b) : value(b ? 1 : 0) {}

    // Default implicit conversions are fine
};

} // namespace jscpp
