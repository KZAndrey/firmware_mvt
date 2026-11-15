#pragma once
#include "MvtWssWire.h"
#include "generated/direct-wss.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>

// Encode straight into caller-owned storage. No document tree or second frame.
namespace MvtWssProto {
inline size_t encode(uint8_t *output,size_t capacity,uint8_t kind,const pb_msgdesc_t *fields,const void *message){
    if(!output||!fields||!message||capacity<8||!kind||kind>MvtWssWire::MaxKind)return 0;
    if(capacity>MvtWssWire::MaxFrame)capacity=MvtWssWire::MaxFrame;
    auto stream=pb_ostream_from_buffer(output+8,capacity-8);
    if(!pb_encode(&stream,fields,message))return 0;
    return MvtWssWire::encode(output,capacity,kind,output+8,stream.bytes_written);
}
struct String {
    const uint8_t *data;size_t size;
    String(const char *text):data(reinterpret_cast<const uint8_t *>(text)),size(text?strlen(text):0){}
    String(const uint8_t *data,size_t size):data(data),size(size){}
};
inline bool writeString(pb_ostream_t *stream,const pb_field_t *field,void *const *arg){
    auto *value=static_cast<const String *>(*arg);
    return value&&(!value->size||value->data)&&pb_encode_tag_for_field(stream,field)&&
        pb_encode_string(stream,value->data,value->size);
}
inline void bind(pb_callback_t &callback,const String &value){
    callback.funcs.encode=writeString;callback.arg=const_cast<String *>(&value);
}
}
