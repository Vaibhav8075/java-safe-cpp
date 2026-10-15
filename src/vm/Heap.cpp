#include "Heap.h"
#include "RuntimeError.h"
#include <climits>

namespace jscpp {

int Heap::allocate(int size) {
    if (size <= 0) {
        throw RuntimeError("Invalid allocation size.");
    }
    if (nextPtr == INT_MAX) {
        throw RuntimeError("OutOfMemoryError: heap handle space exhausted.");
    }
    if (objects.size() >= 1000000) {
        throw RuntimeError("OutOfMemoryError: more than 1,000,000 live objects.");
    }
    int ptr = nextPtr++;
    objects.emplace(ptr, ManagedObject(size));
    counters.allocations++;
    if (static_cast<long long>(objects.size()) > counters.peakLive) {
        counters.peakLive = static_cast<long long>(objects.size());
    }
    return ptr;
}

void Heap::incRef(int ptr) {
    if (ptr == 0) return; // nullptr has no object

    auto it = objects.find(ptr);
    if (it == objects.end()) {
        throw RuntimeError("Internal error: INC_REF on a freed object.");
    }
    it->second.refCount++;
}

void Heap::decRef(int ptr) {
    if (ptr == 0) return; // nullptr has no object

    auto it = objects.find(ptr);
    if (it == objects.end()) {
        throw RuntimeError("Internal error: DEC_REF on a freed object.");
    }
    if (--it->second.refCount <= 0) {
        objects.erase(it); // last reference gone: reclaim immediately
        counters.frees++;
    }
}

ManagedObject& Heap::getObject(int ptr) {
    if (ptr == 0) {
        throw NullPointerException();
    }

    auto it = objects.find(ptr);
    if (it != objects.end()) {
        return it->second;
    }
    throw RuntimeError("UseAfterFreeError: attempted to access freed memory.");
}

void Heap::cleanup() {
    objects.clear();
}

} // namespace jscpp
