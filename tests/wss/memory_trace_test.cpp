#include "../../src/modules/MvtWssMemoryTrace.h"
#include <cassert>
#include <cstdio>
int main(){
    MvtMemoryTrace t; MvtMemoryPoint p;MvtMemoryStat s;
    assert(!t.readPoint(0,p));t.record({20,1,1,1,1});assert(t.count==0);
    for(unsigned i=0;i<50;++i)t.record({i%2,i,100-i,80-i,i});
    assert(t.readPoint(0,p)&&p.uptime==18);
    assert(t.readPoint(31,p)&&p.uptime==49&&!t.readPoint(32,p));
    assert(t.readStat(1,s)&&s.calls==25&&s.free==51&&s.largest==31&&s.bytes==49);
    t.freeze(51);t.record({1,52,0,0,999});t.freeze(99);
    assert(t.frozenAt==51&&t.count==50&&t.readStat(1,s)&&s.bytes==49);
    puts("PASS fixed memory trace: bounds, chronological wrap, minima and first-fault freeze");
}
