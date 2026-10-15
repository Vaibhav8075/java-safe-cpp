#pragma once

#include "Symbol.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

namespace jscpp {

class Scope {
public:
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
    int nextLocalIndex = 0;
};

class SymbolTable {
public:
    SymbolTable();

    void enterScope();
    void leaveScope();

    bool define(std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> resolve(const std::string& name) const;
    std::shared_ptr<Symbol> resolveInCurrentScope(const std::string& name) const;

    const std::unordered_map<std::string, std::shared_ptr<Symbol>>& symbolsInCurrentScope() const {
        return scopes.back().symbols;
    }

    int getNextLocalIndex();

private:
    std::vector<Scope> scopes;
};

} // namespace jscpp
