#include "../../src/modules/MvtWssRxBuffer.h"
#include <cassert>
#include <cstdio>
static bool reject;
static unsigned live, allocations;
static void *resize(void *p,size_t n) {
    ++allocations;
    if(reject)return nullptr;
    void *result=std::realloc(p,n);
    if(result&&!p)++live;
    return result;
}
static void release(void *p){if(p){assert(live);--live;}std::free(p);}
using Buffer=MvtWssRxBufferStorage<resize,release>;
int main(){
    {
        Buffer b;
        reject=true;assert(!b.reserve(8192));assert(b.empty()&&!live);
        reject=false;assert(b.reserve(8192));assert(b.append("hello",5));
        const char *address=b.data();unsigned count=allocations;
        reject=true;assert(!b.reserve(16384));assert(b.data()==address&&b.size()==5);
        assert(!b.append(nullptr,1));assert(!b.append("x",8193));
        assert(!b.reserve(SIZE_MAX));assert(allocations==count+1);
        Buffer queued(std::move(b));assert(b.empty()&&b.capacity()==0);
        assert(queued.data()==address&&!strcmp(queued.c_str(),"hello"));
        assert(live==1);queued.clear();assert(!queued.size()&&!strcmp(queued.c_str(),""));
        Buffer().swap(queued);assert(!live);
        reject=false;assert(b.reserve(8192));
        char payload[8192];std::memset(payload,'a',sizeof(payload));
        assert(b.append(payload,sizeof(payload))&&b.size()==8192);
        assert(b.c_str()[8192]==0&&!std::memcmp(b.data(),payload,8192));
        assert(!b.append("x",1)&&b.size()==8192);
    }
    assert(!live);puts("PASS RX buffer: allocation failure, bounds, terminator, move without copy, release");
}
