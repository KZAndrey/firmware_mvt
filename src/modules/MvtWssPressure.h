#pragma once
#include <stdint.h>
#include <stddef.h>
// USB stress only. Fixed upper bound; release is serviced even after a WSS
// fault. The floor is a pre-allocation guard, not a reservation against peers.
class MvtWssPressure {
    void *blocks[32]{};
    unsigned count=0;
    uint32_t started=0;
    bool running=false;
public:
    template<class Free, class Alloc> unsigned start(uint32_t now,Free freeBytes,Alloc allocate) {
        if(running)return 0;
        running=true;started=now;
        while(count<32 && freeBytes()>=8192+512+64){
            void *p=allocate(512);
            if(!p)break;
            blocks[count++]=p;
        }
        return count*512;
    }
    template<class Release> void cancel(Release release) {
        while(count)release(blocks[--count]);
        running=false;
    }
    template<class Release> bool tick(uint32_t now,Release release) {
        if(!running||uint32_t(now-started)<45000)return false;
        cancel(release);return true;
    }
    bool active() const{return running;}
};
