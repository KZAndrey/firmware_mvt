#pragma once
#include <atomic>
#include <stdint.h>
// Fixed-size, nonblocking USB-test recorder. Contention drops a sample rather
// than delaying WiFi. No pointers, strings, credentials or allocation recorded.
struct MvtMemoryPoint { uint32_t phase, uptime, free, largest, bytes; };
struct MvtMemoryStat { uint32_t calls=0, free=UINT32_MAX, largest=UINT32_MAX, bytes=0; };
class MvtMemoryTrace {
    std::atomic_flag busy=ATOMIC_FLAG_INIT;
public:
    std::atomic<uint32_t> frozenAt{0};
    MvtMemoryPoint points[32]{};
    MvtMemoryStat stats[20]{};
    uint32_t count=0;
    void record(MvtMemoryPoint p) {
        if(p.phase>=20 || frozenAt.load() || busy.test_and_set())return;
        if(!frozenAt.load()){
            points[count++%32]=p;
            auto &s=stats[p.phase];++s.calls;
            if(p.free<s.free)s.free=p.free;
            if(p.largest<s.largest)s.largest=p.largest;
            if(p.bytes>s.bytes)s.bytes=p.bytes;
        }
        busy.clear();
    }
    void freeze(uint32_t uptime) { uint32_t zero=0;frozenAt.compare_exchange_strong(zero,uptime?uptime:1); }
    bool readPoint(unsigned index,MvtMemoryPoint &out) {
        if(busy.test_and_set())return false;
        bool valid=index<(count<32?count:32);
        if(valid)out=points[((count>32?count-32:0)+index)%32];
        busy.clear();return valid;
    }
    bool readStat(unsigned index,MvtMemoryStat &out) {
        if(index>=20||busy.test_and_set())return false;
        out=stats[index];busy.clear();return true;
    }
};
