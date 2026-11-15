#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// direct-wss.proto envelope. Views borrow the received frame; no heap allocation.
namespace MvtWssWire {
constexpr size_t MaxFrame=4096;
constexpr uint8_t Version=1, MaxKind=35;
constexpr uint64_t MaxInteger=9007199254740991ULL;
struct View {uint8_t kind=0;const uint8_t *payload=nullptr;size_t size=0;};
struct Reader {
    const uint8_t *data;size_t size,offset=0;
    Reader(const uint8_t *data,size_t size):data(data),size(size){}
    bool integer(uint64_t &value){
        value=0;
        for(unsigned i=0;i<8;++i){
            if(offset>=size)return false;
            const uint8_t byte=data[offset++];
            value|=uint64_t(byte&127)<<(i*7);
            if(value>MaxInteger)return false;
            if(!(byte&128))return i==0||byte!=0;
        }
        return false;
    }
};
inline size_t integer(uint8_t *out,uint64_t value){
    size_t n=0;
    do{out[n++]=uint8_t(value&127)|(value>=128?128:0);value>>=7;}while(value);
    return n;
}
inline size_t encode(uint8_t *out,size_t capacity,uint8_t kind,const uint8_t *payload,size_t size){
    if(!out||!kind||kind>MaxKind||(!payload&&size)||size>MaxFrame-8)return 0;
    uint8_t length[8];const size_t count=integer(length,size),total=5+count+size;
    if(total>capacity||total>MaxFrame)return 0;
    out[0]=8;out[1]=Version;out[2]=16;out[3]=kind;out[4]=26;
    memcpy(out+5,length,count);if(size)memmove(out+5+count,payload,size);
    return total;
}
inline bool decode(const uint8_t *data,size_t size,View &out){
    out=View{};
    if(!data||size>MaxFrame)return false;
    Reader r(data,size);uint64_t version=0,kind=0;bool seenVersion=false,seenKind=false,seenPayload=false;
    const uint8_t *payload=nullptr;size_t payloadSize=0;
    while(r.offset<size){
        uint64_t tag=0,value=0;if(!r.integer(tag))return false;
        if(tag==8&&!seenVersion){seenVersion=true;if(!r.integer(version))return false;}
        else if(tag==16&&!seenKind){seenKind=true;if(!r.integer(kind))return false;}
        else if(tag==26&&!seenPayload){
            seenPayload=true;if(!r.integer(value)||value>size-r.offset)return false;
            payload=data+r.offset;payloadSize=size_t(value);r.offset+=payloadSize;
        }else return false;
    }
    if(!seenVersion||version!=Version||!seenKind||!kind||kind>MaxKind||!seenPayload)return false;
    out.kind=uint8_t(kind);out.payload=payload;out.size=payloadSize;return true;
}
inline size_t heartbeat(uint8_t *out,size_t capacity,uint64_t timestamp,bool ack=false){
    if(timestamp>MaxInteger)return 0;
    uint8_t payload[9];payload[0]=8;
    const size_t size=1+integer(payload+1,timestamp);
    return encode(out,capacity,ack?5:4,payload,size);
}
inline bool heartbeat(const uint8_t *data,size_t size,uint64_t &timestamp,bool ack=false){
    View view;
    if(!decode(data,size,view)||view.kind!=(ack?5:4))return false;
    Reader r(view.payload,view.size);uint64_t tag=0,value=0;
    if(!r.integer(tag)||tag!=8||!r.integer(value)||r.offset!=r.size)return false;
    timestamp=value;return true;
}
struct Bytes {const uint8_t *data=nullptr;size_t size=0;};
struct FirmwareChunk {Bytes room,job,data;uint32_t node=0,offset=0;};
inline bool token(const Bytes &text,size_t maximum){
    if(!text.data||!text.size||text.size>maximum)return false;
    for(size_t i=0;i<text.size;++i){const uint8_t c=text.data[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    }
    return true;
}
inline bool firmwareChunk(const uint8_t *data,size_t size,FirmwareChunk &out){
    out=FirmwareChunk{};View view;
    if(!decode(data,size,view)||view.kind!=24)return false;
    Reader r(view.payload,view.size);FirmwareChunk chunk;bool node=false,offset=false;
    while(r.offset<r.size){
        uint64_t tag=0,value=0;if(!r.integer(tag))return false;
        if(tag==21&&!node){
            node=true;if(r.size-r.offset<4)return false;
            for(unsigned i=0;i<4;++i)chunk.node|=uint32_t(r.data[r.offset++])<<(i*8);
        }else if(tag==32&&!offset){
            offset=true;if(!r.integer(value)||value>=0x330000)return false;chunk.offset=uint32_t(value);
        }else if(tag==10||tag==26||tag==42){
            Bytes &bytes=tag==10?chunk.room:tag==26?chunk.job:chunk.data;
            if(bytes.data||!r.integer(value)||value>r.size-r.offset)return false;
            bytes.data=r.data+r.offset;bytes.size=size_t(value);r.offset+=bytes.size;
        }else return false;
    }
    if(!node||!chunk.node||!offset||!token(chunk.room,128)||!token(chunk.job,64)||
       !chunk.data.data||!chunk.data.size||chunk.data.size>2048||chunk.data.size>0x330000-chunk.offset)return false;
    out=chunk;return true;
}
}
