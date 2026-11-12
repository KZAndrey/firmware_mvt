#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <utility>

// One owner from assembly through queue consumption. Allocation failure is a
// return value even in embedded builds compiled without C++ exceptions.
template <void *(*Resize)(void *, size_t) = std::realloc,
          void (*Free)(void *) = std::free>
class MvtWssRxBufferStorage {
    char *bytes = nullptr;
    size_t length = 0, reserved = 0;
public:
    MvtWssRxBufferStorage() = default;
    MvtWssRxBufferStorage(const MvtWssRxBufferStorage &) = delete;
    MvtWssRxBufferStorage &operator=(const MvtWssRxBufferStorage &) = delete;
    MvtWssRxBufferStorage(MvtWssRxBufferStorage &&other) noexcept { swap(other); }
    ~MvtWssRxBufferStorage() { Free(bytes); }
    void swap(MvtWssRxBufferStorage &other) noexcept {
        std::swap(bytes, other.bytes);
        std::swap(length, other.length);
        std::swap(reserved, other.reserved);
    }
    bool reserve(size_t n) {
        if (n <= reserved) return true;
        if (n == SIZE_MAX) return false;
        void *p = Resize(bytes, n + 1);
        if (!p) return false;
        bytes = static_cast<char *>(p); reserved = n; bytes[length] = 0;
        return true;
    }
    bool append(const char *p, size_t n) {
        if (n > reserved - length || (!p && n)) return false;
        if (n) std::memcpy(bytes + length, p, n);
        length += n;
        if (bytes) bytes[length] = 0;
        return true;
    }
    void clear() { length = 0; if (bytes) bytes[0] = 0; }
    size_t size() const { return length; }
    size_t capacity() const { return reserved; }
    bool empty() const { return !length; }
    const char *data() const { return bytes ? bytes : ""; }
    const char *c_str() const { return data(); }
};
using MvtWssRxBuffer = MvtWssRxBufferStorage<>;
