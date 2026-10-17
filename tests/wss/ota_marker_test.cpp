#include <cassert>
#include <cstdlib>
#include <new>
#include <cstdio>
#include <cstring>
#include "../../src/modules/MvtOtaMarker.h"

static bool denyHeap = false;
void *operator new(size_t n) {
    assert(!denyHeap);
    if (void *p = malloc(n)) return p;
    throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }

int main() {
    const char pattern[] = "MVTOTA1:heltec-v3:8:2.7.15.1610267";
    denyHeap = true;
    // Every possible split, including the required NUL in the next chunk.
    for (size_t split = 0; split < sizeof(pattern); ++split) {
        MvtOtaMarker m(pattern, sizeof(pattern));
        assert(m.valid());
        assert(!m.feed(reinterpret_cast<const uint8_t *>(pattern), split));
        assert(m.feed(reinterpret_cast<const uint8_t *>(pattern) + split, sizeof(pattern) - split));
    }
    MvtOtaMarker m(pattern, sizeof(pattern));
    uint8_t chunk[2048] = {};
    // Simulate a full image with heap allocations forbidden throughout.
    for (int i = 0; i < 900; ++i) assert(!m.feed(chunk, sizeof(chunk)));
    const char wrong[] = "MVTOTA1:heltec-v3:7:2.7.15.1610266";
    assert(!m.feed(reinterpret_cast<const uint8_t *>(wrong), sizeof(wrong)));
    assert(!m.feed(reinterpret_cast<const uint8_t *>(pattern), sizeof(pattern) - 1));
    const uint8_t nul = 0;
    assert(m.feed(&nul, 1));
    assert(m.feed(chunk, sizeof(chunk)));
    const char overlap[] = "ababac";
    MvtOtaMarker repeated(overlap, sizeof(overlap) - 1);
    const uint8_t input[] = "ababababac";
    assert(repeated.feed(input, sizeof(input) - 1));
    assert(!MvtOtaMarker(nullptr, 1).valid());
    assert(!MvtOtaMarker(pattern, 0).valid());
    assert(!MvtOtaMarker(pattern, 97).valid());
    denyHeap = false;
    puts("OTA marker: all boundaries, binary NUL, wrong version, full image without heap allocations: PASS");
}
