#pragma once
#include "MvtWssMessageCodec.h"
#include "platform/esp32/MvtWssDiagnostics.h"

// Native connection evidence is encoded directly. Do not expand diagnostics
// into cJSON and then parse/serialize them again while WiFi needs the same heap.
namespace MvtWssMetadata {
using MvtWssMessageCodec::Writer;
inline bool number(Writer &w,unsigned tag,uint64_t value){return w.integer(tag*8)&&w.integer(value);}
inline bool signedNumber(Writer &w,unsigned tag,int32_t value){const int64_t v=value;return number(w,tag,v<0?uint64_t(-v*2-1):uint64_t(v*2));}
inline bool fixed(Writer &w,unsigned tag,uint32_t value){uint8_t b[4];for(unsigned i=0;i<4;++i)b[i]=uint8_t(value>>(8*i));return w.integer(tag*8+5)&&w.put(b,4);}
inline bool text(Writer &w,unsigned tag,const char *value){
    if(!value)return true;size_t n=strlen(value);
    return n<=512&&MvtWssMessageCodec::utf8(reinterpret_cast<const uint8_t *>(value),n)&&
        w.integer(tag*8+2)&&w.integer(n)&&w.put(value,n);
}
template<class Encode> bool nested(Writer &w,unsigned tag,const Encode &encode){
    Writer size(nullptr,MvtWssWire::MaxFrame);
    return encode(size)&&w.integer(tag*8+2)&&w.integer(size.size)&&encode(w);
}
inline bool diagnostic(Writer &w,const MvtWssDiagnostic &d){
    return fixed(w,1,d.bootId)&&number(w,2,d.sequence)&&number(w,3,d.uptimeMs)&&text(w,4,d.stage)&&
        signedNumber(w,5,d.ret)&&signedNumber(w,6,d.error)&&signedNumber(w,7,d.socketError)&&
        signedNumber(w,8,d.tlsError)&&signedNumber(w,9,d.tlsCode)&&signedNumber(w,10,d.tlsFlags)&&
        signedNumber(w,11,d.opcode)&&signedNumber(w,12,d.payload)&&signedNumber(w,13,d.offset)&&
        number(w,14,d.duringSession!=0)&&number(w,15,d.freeHeap)&&number(w,16,d.largestBlock)&&number(w,17,d.minimumHeap);
}
inline bool boot(Writer &w,const MvtWssBootDiagnostic &d){
    return fixed(w,1,d.bootId)&&number(w,2,d.resetReason)&&number(w,3,d.wakeCause)&&number(w,4,d.bootCount)&&
        number(w,5,d.previousSleepUptimeMs)&&number(w,6,d.previousSleepDurationMs)&&number(w,7,d.sleepIntent);
}
struct Info {
    const cJSON *node=nullptr;
    const char *nodeName=nullptr,*version=nullptr,*frequency=nullptr,*channel=nullptr,*region=nullptr,*preset=nullptr;
    bool radio=false,tx=false,ota=false,hasDisconnect=false,hasAttempt=false,hasBoot=false;
    uint32_t hopLimit=0,generation=0;
    uint64_t sampledAt=0;
    MvtWssDiagnostic disconnect{},attempt{};
    MvtWssBootDiagnostic startup{};
};
inline bool body(Writer &w,const Info &info){
    const auto &update=MvtWssSchema::messages[MvtWssSchema::bodies[7]];
    return nested(w,1,[&](Writer &out){return MvtWssMessageCodec::body(out,update.fields[0].sub,info.node,0);})&&
        text(w,2,info.nodeName)&&text(w,3,info.version)&&number(w,4,info.radio)&&number(w,5,info.tx)&&
        number(w,6,info.hopLimit)&&number(w,7,info.hopLimit)&&text(w,8,info.frequency)&&text(w,9,info.channel)&&
        text(w,10,info.region)&&text(w,11,info.preset)&&number(w,12,info.ota)&&number(w,13,info.generation)&&
        (!info.hasDisconnect||nested(w,14,[&](Writer &out){return diagnostic(out,info.disconnect);}))&&
        (!info.hasAttempt||nested(w,15,[&](Writer &out){return diagnostic(out,info.attempt);}))&&
        (!info.hasBoot||nested(w,16,[&](Writer &out){return boot(out,info.startup);}))&&number(w,17,info.sampledAt);
}
inline size_t encode(uint8_t *out,size_t capacity,const Info &info){
    if(capacity<8)return 0;if(capacity>MvtWssWire::MaxFrame)capacity=MvtWssWire::MaxFrame;
    Writer w(out?out+8:nullptr,capacity-8);if(!body(w,info))return 0;
    return out?MvtWssWire::encode(out,capacity,6,out+8,w.size):w.size+8;
}
inline size_t auth(uint8_t *out,size_t capacity,const char *room,const char *key,uint32_t node,const char *name,const char *version){
    if(!out||capacity<8)return 0;Writer w(out+8,capacity-8);
    if(!text(w,1,room)||!text(w,2,key)||!fixed(w,3,node)||!text(w,4,name)||!text(w,5,version))return 0;
    return MvtWssWire::encode(out,capacity,1,out+8,w.size);
}
}
