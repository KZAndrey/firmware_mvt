#include "../../src/modules/MvtWssPressure.h"
#include <cassert>
#include <cstdio>
int main(){
    MvtWssPressure p;
    size_t free=20000;unsigned allocated=0,released=0;
    auto available=[&](){return free;};
    auto alloc=[&](size_t n)->void*{free-=n;return reinterpret_cast<void*>(uintptr_t(++allocated));};
    auto release=[&](void *v){assert(v);++released;free+=512;};
    unsigned bytes=p.start(0xfffffff0,available,alloc);
    assert(bytes>0&&free>=8192&&allocated<=32);
    assert(p.start(1,available,alloc)==0);
    assert(!p.tick(uint32_t(0xfffffff0+44999u),release));
    assert(p.tick(uint32_t(0xfffffff0+45000u),release));
    assert(free==20000&&released==allocated&&!p.active());
    p.cancel(release);assert(released==allocated);
    free=100000;allocated=released=0;
    assert(p.start(0,available,alloc)==16384);p.cancel(release);assert(released==32);
    free=8000;allocated=released=0;
    assert(p.start(0,available,alloc)==0);p.cancel(release);assert(allocated==0);
    free=20000;allocated=released=0;
    assert(p.start(0,available,[&](size_t n)->void*{return allocated==3?nullptr:alloc(n);})==1536);
    p.cancel(release);assert(released==3&&free==20000);
    puts("PASS bounded DMA pressure: floor, cap, OOM, duplicate start, cancel, expiry and clock wrap");
}
