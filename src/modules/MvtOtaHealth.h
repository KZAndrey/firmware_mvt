#pragma once
#include <cstdint>

namespace mvtOtaHealth {
// Test the running, already connected gateway, not the memory needed to start
// another TLS handshake. Require fresh server traffic and usable allocation
// headroom throughout the updater's 30-second probation period.
inline bool ready(bool authenticated, bool connected, bool radioReady,
                  uint32_t ackAge, uint32_t freeHeap, uint32_t largestBlock) {
    return authenticated && connected && radioReady && ackAge < 30000 &&
           freeHeap >= 16384 && largestBlock >= 4096;
}
}
