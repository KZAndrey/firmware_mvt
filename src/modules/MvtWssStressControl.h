#pragma once
#include <stdint.h>
#include <stddef.h>
#include <atomic>

// USB-only test protocol: an exact ToRadio heartbeat envelope, never a radio
// packet or network command. Compiled into the console only for stress builds.
inline unsigned mvtStressDecode(const uint8_t *p, size_t n) {
    if (!p || n < 4 || n > 8 || p[0] != 0x3a || p[1] != n-2 || p[2] != 8) return 0;
    uint32_t value=0;
    for (size_t i=3;i<n;++i) {
        const unsigned shift=unsigned(i-3)*7;
        if (shift==28 && (p[i]&0xf0)) return 0;
        value |= uint32_t(p[i]&0x7f)<<shift;
        if (!(p[i]&0x80)) {
            if (i!=n-1 || (value&0xffffff00u)!=0x4d560000u) return 0;
            const unsigned command=value&0xff;
            return command>=1 && command<=18 ? command : 0;
        }
    }
    return 0;
}
inline uint32_t mvtStressDuration(unsigned command) {
    return command==3 ? 5000 : command==4 ? 30000 : command==5 ? 120000 : command==7 ? 60000 : 0;
}
inline bool mvtStressBefore(uint32_t now,uint32_t deadline) {
    return deadline && int32_t(deadline-now)>0;
}
#if MVT_WSS_STRESS_TEST
extern std::atomic<unsigned> mvtStressMailbox;
extern std::atomic<uint32_t> mvtStressWifiUntil;
extern std::atomic<bool> mvtStressRadioReset;
extern std::atomic<unsigned> mvtStressTxLimit;
#endif
