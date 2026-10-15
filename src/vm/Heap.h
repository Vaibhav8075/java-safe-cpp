#pragma once

#include "RuntimeValue.h"
#include <unordered_map>
#include <vector>

namespace jscpp {

struct ManagedObject {
    int refCount;
    int size;
    std::vector<int> data;
    bool isAlive;

    ManagedObject(int size) : refCount(0), size(size), data(size, 0), isAlive(true) {}
};

class Heap {
public:
    Heap() = default;

    int allocate(int size);
    void incRef(int ptr);
    void decRef(int ptr);

    ManagedObject& getObject(int ptr);

    void cleanup(); // Delete all regardless of refcount for exit

private:
    std::unordered_map<int, ManagedObject> objects;
    int nextPtr = 1; // 0 is null
};

} // namespace jscpp
