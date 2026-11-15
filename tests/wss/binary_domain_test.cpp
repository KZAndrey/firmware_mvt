#include "modules/MvtWssMessageCodec.h"
#include "modules/MvtWssMetadata.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
using namespace MvtWssMessageCodec;
static unsigned allocations=0,live=0,failAt=0;
static void *tracked(size_t size){++allocations;if(failAt==allocations)return nullptr;void *p=malloc(size);if(p)++live;return p;}
static void released(void *p){if(p){assert(live);--live;free(p);}}
int main(int argc,char **argv){
    assert(argc==3);std::ifstream input(argv[1]);std::ofstream output(argv[2]);std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        cJSON *fixture=cJSON_Parse(line.c_str());assert(fixture);
        auto *name=cJSON_GetObjectItemCaseSensitive(fixture,"type");auto *value=cJSON_GetObjectItemCaseSensitive(fixture,"body");
        const uint8_t type=kind(name->valuestring);assert(type);
        uint8_t buffer[4096];
        cJSON_Hooks hooks{tracked,released};cJSON_InitHooks(&hooks);allocations=0;
        const size_t bound=encode(nullptr,sizeof(buffer),type,value),size=encode(buffer,sizeof(buffer),type,value);
        assert(bound&&size&&size<=bound&&allocations==0);cJSON_InitHooks(nullptr);
        for(size_t i=0;i<size;++i){char hex[3];snprintf(hex,sizeof(hex),"%02x",buffer[i]);output<<hex;}output<<'\n';
        // These same fields must be accepted by the firmware command decoder.
        if(type!=24){
            cJSON_InitHooks(&hooks);allocations=0;
            auto *decoded=decode(buffer,size);assert(decoded);const unsigned count=allocations;cJSON_Delete(decoded);assert(!live);
            for(unsigned fault=1;fault<=count;++fault){allocations=0;failAt=fault;decoded=decode(buffer,size);assert(!decoded);assert(!live);}
            failAt=0;cJSON_InitHooks(nullptr);
        }
        for(size_t n=0;n<size;++n){auto *bad=decode(buffer,n);assert(!bad);}
        cJSON_Delete(fixture);++cases;
    }
    assert(cases>=35);
    {
        auto *node=cJSON_Parse("{\"nodeId\":3252929280,\"radioCharging\":false,\"heartRate\":0}");assert(node);
        MvtWssMetadata::Info info;info.node=node;info.nodeName="Native";info.version="2.7.15.15.11b";
        info.hasDisconnect=info.hasAttempt=info.hasBoot=true;info.ota=true;info.generation=40;
        info.disconnect.bootId=42;info.disconnect.sequence=3;info.disconnect.ret=-1;
        strcpy(info.disconnect.stage,"read");info.disconnect.duringSession=1;
        info.disconnect.freeHeap=17116;info.disconnect.largestBlock=6132;info.disconnect.minimumHeap=2708;
        info.attempt=info.disconnect;info.attempt.sequence=4;info.attempt.duringSession=0;
        info.startup.bootId=42;info.startup.bootCount=2;info.startup.resetReason=8;
        uint8_t buffer[1024];cJSON_Hooks hooks{tracked,released};cJSON_InitHooks(&hooks);allocations=0;
        const size_t bound=MvtWssMetadata::encode(nullptr,sizeof(buffer),info);
        const size_t size=MvtWssMetadata::encode(buffer,sizeof(buffer),info);
        assert(size&&size<=bound&&!allocations);
        std::ofstream native(std::string(argv[2])+".native",std::ios::binary);native.write(reinterpret_cast<char *>(buffer),size);
        assert(MvtWssMetadata::auth(buffer,sizeof(buffer),"ROOM","test-key",42,"Native","2.7.15.15.11b"));
        assert(!allocations);cJSON_InitHooks(nullptr);cJSON_Delete(node);
    }
    // Repeated, unknown, malformed and overlong fields are rejected, not ignored.
    for(const auto &payload:std::vector<std::vector<uint8_t>>{{8,1,8,2},{0xf8,7,1},{10,2,0xc0,0x80},{10,1,0}}){
        uint8_t frame[64];size_t n=MvtWssWire::encode(frame,sizeof(frame),3,payload.data(),payload.size());
        assert(n&&!decode(frame,n));
    }
    const char *badUtf8[]={"\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\x80"};
    for(const char *p:badUtf8)assert(!utf8(reinterpret_cast<const uint8_t *>(p),strlen(p)));
    uint8_t raw[256];size_t length=0;
    for(const char *p:{"A===","AB==","AAA=AAAA","AA=A","AAAA=","____"})assert(!bytes(p,false,raw,length));
    puts("PASS bounded firmware domain protobuf adapter: zero-allocation encoding, cross-language fixtures, malformed input");
}
