#pragma once
#include <atomic>
#include <cstdlib>
#include <cstddef>

// Task-context only. Retain the static pool's admission limit without
// reserving payload memory while the optional consumer is unused.
template <class T, size_t Limit, void *(*Allocate)(size_t) = std::malloc,
          void (*Free)(void *) = std::free>
class BoundedHeapPool {
    std::atomic<size_t> used{0};
public:
    T *allocate() {
        size_t n=used.load();
        do { if(n>=Limit)return nullptr; } while(!used.compare_exchange_weak(n,n+1));
        T *p=static_cast<T *>(Allocate(sizeof(T)));
        if(!p)used.fetch_sub(1);
        return p;
    }
    void release(T *p) {
        if(!p)return;
        Free(p); used.fetch_sub(1);
    }
};
