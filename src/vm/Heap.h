#pragma once

#include "RuntimeValue.h"
#include <unordered_map>
#include <vector>

namespace jscpp {

struct ManagedObject {
    int refCount;
    int size;
    std::vector<int> data;

    ManagedObject(int size) : refCount(1), size(size), data(size, 0) {}
};

struct HeapStats {
    long long allocations = 0;
    long long frees = 0;
    long long peakLive = 0;
};

class Heap {
public:
    Heap() = default;

    // Returns a handle with refcount 1. Handles are never reused, so a stale handle
    // can always be detected instead of silently aliasing a newer object.
    int allocate(int size);
    void incRef(int ptr);
    void decRef(int ptr);

    ManagedObject& getObject(int ptr);

    size_t liveObjects() const { return objects.size(); }
    const HeapStats& stats() const { return counters; }

    void cleanup(); // Delete all regardless of refcount for exit

private:
    std::unordered_map<int, ManagedObject> objects;
    int nextPtr = 1; // 0 is null
    HeapStats counters;
};

} // namespace jscpp
