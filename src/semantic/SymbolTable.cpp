#include "SymbolTable.h"

namespace jscpp {

SymbolTable::SymbolTable() {
    // Global scope
    scopes.push_back(Scope());
}

void SymbolTable::enterScope() {
    Scope newScope;
    if (!scopes.empty()) {
        newScope.nextLocalIndex = scopes.back().nextLocalIndex;
    }
    scopes.push_back(newScope);
}

void SymbolTable::leaveScope() {
    if (scopes.size() > 1) { // Never pop global scope
        scopes.pop_back();
    }
}

bool SymbolTable::define(std::shared_ptr<Symbol> symbol) {
    auto& currentScope = scopes.back();
    if (currentScope.symbols.find(symbol->name) != currentScope.symbols.end()) {
        return false; // Already defined in current scope
    }

    if (symbol->kind == SymbolKind::Variable && symbol->localIndex == -1) {
        symbol->localIndex = currentScope.nextLocalIndex++;

        // Propagate nextLocalIndex back up to handle nested blocks extending frame size
        for (auto& s : scopes) {
            if (s.nextLocalIndex < currentScope.nextLocalIndex) {
                s.nextLocalIndex = currentScope.nextLocalIndex;
            }
        }
    }

    currentScope.symbols[symbol->name] = symbol;
    return true;
}

std::shared_ptr<Symbol> SymbolTable::resolve(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->symbols.find(name);
        if (found != it->symbols.end()) {
            return found->second;
        }
    }
    return nullptr;
}

std::shared_ptr<Symbol> SymbolTable::resolveInCurrentScope(const std::string& name) const {
    const auto& currentScope = scopes.back();
    auto found = currentScope.symbols.find(name);
    if (found != currentScope.symbols.end()) {
        return found->second;
    }
    return nullptr;
}

int SymbolTable::getNextLocalIndex() {
    return scopes.back().nextLocalIndex;
}

} // namespace jscpp
