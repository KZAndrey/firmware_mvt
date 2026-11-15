#include "modules/MvtWssProto.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
void *operator new(size_t){std::abort();}
void *operator new[](size_t){std::abort();}
int main(int argc,char **argv){
    assert(argc==2);
    using namespace MvtWssProto;
    uint8_t frame[4096];
    movetain_direct_v1_GatewayInfo info=movetain_direct_v1_GatewayInfo_init_zero;
    info.has_node=true;info.node.nodeId=0xc1e3a300;
    info.node.has_temp=true;info.node.temp=-12.5;
    info.node.has_radioCharging=true;info.node.radioCharging=false;
    String name("WSGT"),version("2.7.15.15.11b"),stage("ping_write");
    bind(info.nodeName,name);bind(info.firmwareVersion,version);
    info.has_lastDisconnectDiag=true;
    info.lastDisconnectDiag.ret=-1;info.lastDisconnectDiag.bootId=0xffffffff;
    bind(info.lastDisconnectDiag.stage,stage);
    info.otaGeneration=40;info.radioConnected=true;info.sampledAt=1794718800000ULL;
    const size_t n=encode(frame,sizeof(frame),6,&movetain_direct_v1_GatewayInfo_msg,&info);
    assert(n&&n<256);
    FILE *file=fopen(argv[1],"wb");assert(file);assert(fwrite(frame,1,n,file)==n);fclose(file);
    for(size_t capacity=0;capacity<n;++capacity)assert(!encode(frame,capacity,6,&movetain_direct_v1_GatewayInfo_msg,&info));
    printf("PASS nanopb gateway metadata without C++ allocation: %zu bytes\n",n);
}
