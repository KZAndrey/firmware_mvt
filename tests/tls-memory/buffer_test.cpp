#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <set>
#include <thread>
#include <vector>
#include "platform/esp32/MvtTlsMemory.h"
#include "platform/esp32/MvtTlsBufferPool.h"
#include "freertos/task.h"
#include "mbedtls/ssl_internal.h"
#include "mbedtls/platform.h"
#include "esp_mem.h"

thread_local TaskHandle_t testCurrentTask = reinterpret_cast<void *>(1);
bool fragmented = true;
unsigned fallbackCalls = 0, installs = 0;
std::set<void *> fallbackPointers;
void *esp_mbedtls_mem_calloc(size_t count, size_t size) {
    ++fallbackCalls;
    // Enough aggregate memory may exist while no contiguous record buffer fits.
    if (fragmented && count * size >= MBEDTLS_SSL_IN_BUFFER_LEN) return nullptr;
    void *p = std::calloc(count, size);
    if (p) fallbackPointers.insert(p);
    return p;
}
void esp_mbedtls_mem_free(void *p) {
    assert(fallbackPointers.erase(p) == 1); // never pass reserved memory to heap free
    std::free(p);
}
void *(*allocate)(size_t, size_t) = esp_mbedtls_mem_calloc;
void (*release)(void *) = esp_mbedtls_mem_free;
int mbedtls_platform_set_calloc_free(void *(*a)(size_t, size_t), void (*f)(void *)) {
    ++installs; allocate=a; release=f; return 0;
}
int main() {
    constexpr size_t bytes = MBEDTLS_SSL_IN_BUFFER_LEN;
    assert(!allocate(1, bytes)); // negative control: ordinary allocator cannot fit TLS record
    assert(mvt_tls_memory_init() && mvt_tls_memory_init() && installs == 1);
    mvt_tls_memory_set_task(testCurrentTask);
    for (unsigned cycle=0; cycle<1000; ++cycle) {
        const auto before = fallbackCalls;
        auto *in = static_cast<unsigned char *>(allocate(1, bytes));
        auto *out = static_cast<unsigned char *>(allocate(bytes, 1));
        assert(in && out && in != out && fallbackCalls == before);
        assert(reinterpret_cast<uintptr_t>(in)%16 == 0 && reinterpret_cast<uintptr_t>(out)%16 == 0);
        for (size_t i=0; i<bytes; ++i) assert(in[i]==0 && out[i]==0);
        std::memset(in, 0x71, bytes); std::memset(out, 0x82, bytes);
        assert(!allocate(1, bytes)); // pool exhaustion cannot alias a live record
        void *small = allocate(1, 384); assert(small); release(small);
        // Actual transport destroy runs on the gateway control task, not TLS task.
        testCurrentTask=reinterpret_cast<void *>(2);
        release(out); release(in); release(nullptr);
        assert(!allocate(1, bytes)); // unrelated TLS users cannot take gateway reserve
        testCurrentTask=reinterpret_cast<void *>(1);
    }
    const auto before = fallbackCalls;
    assert(!allocate(SIZE_MAX, 2) && fallbackCalls == before);
    mvt_tls_memory_set_task(nullptr);
    assert(!allocate(1, bytes));
    fragmented=false;
    void *ordinary=allocate(1, bytes); assert(ordinary); release(ordinary);
    assert(fallbackPointers.empty());

    // Exercise ownership transfer while a different task frees a buffer.
    MvtTlsBufferPool<16717> pool;
    std::vector<std::thread> workers;
    for (unsigned t=0;t<4;++t) workers.emplace_back([&] {
        for (unsigned i=0;i<250;++i) {
            void *p=nullptr;
            while (!(p=pool.allocate(bytes))) std::this_thread::yield();
            auto *data=static_cast<unsigned char *>(p);
            for(size_t j=0;j<bytes;++j) assert(data[j]==0);
            std::memset(p,0x93,bytes); assert(pool.release(p));
        }
    });
    for(auto &worker:workers)worker.join();
    puts("TLS buffer reserve: fragmented-heap negative control, 1000 reuse cycles, zeroing, task isolation, fallback, overflow and concurrent ownership: PASS");
}
