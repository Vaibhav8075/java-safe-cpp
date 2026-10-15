#include "Heap.h"
#include "RuntimeError.h"
#include <iostream>

namespace jscpp {

int Heap::allocate(int size) {
    if (size <= 0) {
        throw RuntimeError("Invalid allocation size.");
    }
    int ptr = nextPtr++;
    objects.emplace(ptr, ManagedObject(size));
    return ptr;
}

void Heap::incRef(int ptr) {
    if (ptr == 0) return; // Nullptr check

    auto it = objects.find(ptr);
    if (it != objects.end() && it->second.isAlive) {
        it->second.refCount++;
    } else {
        throw RuntimeError("Attempted to incRef an invalid or dead object.");
    }
}

void Heap::decRef(int ptr) {
    if (ptr == 0) return; // Nullptr check

    auto it = objects.find(ptr);
    if (it != objects.end() && it->second.isAlive) {
        it->second.refCount--;
        if (it->second.refCount <= 0) {
            it->second.isAlive = false;
            objects.erase(it); // Reclaim memory
        }
    } else {
        // Can happen if something is messed up, but let's fail safely
        // Just ignore or throw
        throw RuntimeError("Attempted to decRef an invalid or dead object.");
    }
}

ManagedObject& Heap::getObject(int ptr) {
    if (ptr == 0) {
        throw NullPointerException();
    }

    auto it = objects.find(ptr);
    if (it != objects.end() && it->second.isAlive) {
        return it->second;
    }
    throw RuntimeError("Attempted to access deallocated memory.");
}

void Heap::cleanup() {
    objects.clear();
}

} // namespace jscpp
