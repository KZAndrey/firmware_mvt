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
        // A local failure to start the receiver does not test the network.
        // Preserve existing DNS/TCP evidence, but never count this as another
        // network failure (or trigger a rejoin for startup failures alone).
        if (!d.duringSession && (!std::strcmp(d.stage, "client_start") ||
            !std::strcmp(d.stage, "client_init") || !std::strcmp(d.stage, "worker_task") ||
            !std::strcmp(d.stage, "worker_resources"))) return;
        // Pinned ESP-IDF: 0x8001 = DNS resolution, 0x8006 = connection timeout.
        // Never rejoin for certificate, authentication, read or write failures.
        if (std::strcmp(d.stage, "connect") || d.duringSession ||
            (d.tlsError != 0x8001 && d.tlsError != 0x8006)) { clear(); return; }
        if (!failures) firstFailure = now;
        if (failures < 3) ++failures;
    }
    bool due(uint32_t now) const {
        return failures >= 3 && attemptsThisOutage < 2 && uint32_t(now-firstFailure) >= 120000 &&
            (!hasRecovered || uint32_t(now-lastRecovery) >= (attemptsThisOutage ? 600000u : 120000u));
    }
    void recovered(uint32_t now) { ++attemptsThisOutage; hasRecovered=true; lastRecovery=now; clear(); }
    void clear() { failures=0; firstFailure=0; }
    void healthy() { clear(); attemptsThisOutage=0; }
private:
    uint32_t seen=0, firstFailure=0, lastRecovery=0;
    unsigned failures=0;
    unsigned attemptsThisOutage=0;
    bool hasRecovered=false;
};
