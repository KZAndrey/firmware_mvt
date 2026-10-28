#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Two reusable TLS record buffers, independent of the general heap's layout.
// Ownership is transferred atomically; clearing happens before releasing a slot.
template <size_t Bytes> class MvtTlsBufferPool {
    static constexpr size_t Stride = (Bytes + 15) & ~size_t(15);
    alignas(16) unsigned char buffers[2][Stride] = {};
    uint32_t used = 0;
public:
    void *allocate(size_t bytes) {
        if (bytes != Bytes) return nullptr;
        uint32_t previous = __atomic_load_n(&used, __ATOMIC_RELAXED);
        for (;;) {
            if ((previous & 3) == 3) return nullptr;
            const uint32_t bit = (previous & 1) ? 2 : 1;
            if (__atomic_compare_exchange_n(&used, &previous, previous | bit, false,
                                            __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                void *result = buffers[bit == 1 ? 0 : 1];
                std::memset(result, 0, Bytes);
                return result;
            }
        }
    }
    bool release(void *pointer) {
        for (unsigned i = 0; i < 2; ++i) {
            if (pointer != buffers[i]) continue;
            // Do not leave plaintext or TLS record material in a reserved slot.
            volatile unsigned char *bytes = buffers[i];
            for (size_t j = 0; j < Stride; ++j) bytes[j] = 0;
            __atomic_fetch_and(&used, ~(uint32_t(1) << i), __ATOMIC_RELEASE);
            return true;
        }
        return false;
    }
};
