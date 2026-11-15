#include "modules/MvtWssWire.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <initializer_list>
// The complete codec must remain usable with allocation unavailable.
void *operator new(size_t){std::abort();}
void *operator new[](size_t){std::abort();}
int main(int argc,char **argv){
    using namespace MvtWssWire;
    uint8_t frame[4096],payload[2048];
    for(size_t i=0;i<sizeof(payload);++i)payload[i]=uint8_t(i);
    for(unsigned k=1;k<=MaxKind;++k){
        const size_t n=encode(frame,sizeof(frame),k,payload,sizeof(payload));
        View v;assert(n&&decode(frame,n,v));assert(v.kind==k&&v.size==sizeof(payload));
        assert(!memcmp(v.payload,payload,v.size));
        for(size_t cut=0;cut<n;++cut)assert(!decode(frame,cut,v));
    }
    const uint64_t times[]={0,1,127,128,4294967295ULL,1794718800000ULL,MaxInteger};
    for(auto stamp:times)for(bool ack:{false,true}){
        const size_t n=heartbeat(frame,sizeof(frame),stamp,ack);assert(n&&n<=15);
        uint64_t restored=0;
        assert(heartbeat(static_cast<const uint8_t *>(frame),n,restored,ack)&&restored==stamp);
        assert(!heartbeat(static_cast<const uint8_t *>(frame),n,restored,!ack));
        printf("%llu %u ",(unsigned long long)stamp,unsigned(ack));
        for(size_t i=0;i<n;++i)printf("%02x",frame[i]);puts("");
        for(size_t cap=0;cap<n;++cap)assert(!heartbeat(frame,cap,stamp,ack));
    }
    assert(!heartbeat(frame,sizeof(frame),MaxInteger+1));
    assert(!encode(frame,sizeof(frame),0,payload,1));
    assert(!encode(frame,sizeof(frame),MaxKind+1,payload,1));
    assert(!encode(frame,sizeof(frame),1,nullptr,1));
    const uint8_t invalid[][9]={{8,2,16,4,26,0},{8,1,16,4,26,128,0},
        {8,1,16,4,26,0,8,1},{8,1,16,4,26,0,32,0}};
    for(const auto &bytes:invalid){View v;assert(!decode(bytes,sizeof(bytes),v));}
    if(argc>1){
        FILE *file=fopen(argv[1],"rb");assert(file);
        const size_t size=fread(frame,1,sizeof(frame),file);assert(feof(file));fclose(file);
        FirmwareChunk chunk;assert(firmwareChunk(frame,size,chunk));
        assert(chunk.node==0xc1e3a300&&chunk.offset==4096&&chunk.data.size==2048);
        assert(chunk.room.size==10&&!memcmp(chunk.room.data,"IDCC0A8F28",10));
        for(size_t i=0;i<chunk.data.size;++i)assert(chunk.data.data[i]==uint8_t(i));
        for(size_t cut=0;cut<size;++cut)assert(!firmwareChunk(frame,cut,chunk));
    }
}
