#pragma once
#include "../platform/esp32/MvtWssDiagnostics.h"
#include <cstring>

// WiFi can keep WL_CONNECTED even when local IP traffic no longer works.
// A bounded rejoin is a recovery attempt, not proof of the network fault's cause.
class MvtWssNetworkRecovery {
public:
    void observe(const MvtWssDiagnostic &d, uint32_t now) {
        if (!d.sequence || d.sequence == seen) return;
        seen = d.sequence;
        // Pinned ESP-IDF: 0x8001 = DNS resolution, 0x8006 = connection timeout.
        // Never rejoin for certificate, authentication, read or write failures.
        if (std::strcmp(d.stage, "connect") || d.duringSession ||
            (d.tlsError != 0x8001 && d.tlsError != 0x8006)) { clear(); return; }
        if (!failures) firstFailure = now;
        if (failures < 3) ++failures;
    }
    bool due(uint32_t now) const {
        return failures >= 3 && !usedThisOutage && uint32_t(now-firstFailure) >= 120000 &&
            (!hasRecovered || uint32_t(now-lastRecovery) >= 600000);
    }
    void recovered(uint32_t now) { usedThisOutage=true; hasRecovered=true; lastRecovery=now; clear(); }
    void clear() { failures=0; firstFailure=0; }
    void healthy() { clear(); usedThisOutage=false; }
private:
    uint32_t seen=0, firstFailure=0, lastRecovery=0;
    unsigned failures=0;
    bool usedThisOutage=false, hasRecovered=false;
};
