#pragma once

#include "Type.h"
#include <string>
#include <vector>

namespace jscpp {

enum class SymbolKind {
    Variable,
    Function
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    std::shared_ptr<Type> type;

    // For functions
    std::vector<std::shared_ptr<Type>> paramTypes;

    // For code generation
    int localIndex = -1;
};

} // namespace jscpp
