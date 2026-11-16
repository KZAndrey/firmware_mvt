#pragma once
#include "MvtWssWire.h"
#include "generated/MvtWssMessageSchema.h"
#include <cJSON.h>
#include <cmath>
#include <cstdlib>

// Domain adapter for existing radio handlers. The wire contains typed protobuf
// fields, never JSON text. Encoding writes directly to caller-owned storage;
// keepalive and OTA blocks bypass the document adapter entirely.
namespace MvtWssMessageCodec {
using namespace MvtWssSchema;
struct Writer {
    uint8_t *data; size_t capacity, size=0;
    Writer(uint8_t *data,size_t capacity):data(data),capacity(capacity){}
    bool put(const void *p,size_t n){
        if(n>capacity-size)return false;
        if(data&&n)memcpy(data+size,p,n);size+=n;return true;
    }
    bool integer(uint64_t v){uint8_t b[10];return put(b,MvtWssWire::integer(b,v));}
};
inline bool utf8(const uint8_t *p,size_t n){
    for(size_t i=0;i<n;){
        uint32_t c=p[i++];if(!c)return false;if(c<128)continue;
        unsigned count;uint32_t minimum;
        if(c>=0xc2&&c<=0xdf){count=1;minimum=0x80;c&=31;}
        else if(c>=0xe0&&c<=0xef){count=2;minimum=0x800;c&=15;}
        else if(c>=0xf0&&c<=0xf4){count=3;minimum=0x10000;c&=7;}
        else return false;
        if(count>n-i)return false;
        while(count--){const uint8_t b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}
        if(c<minimum||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;
    }return true;
}
// Match JavaScript/Kotlin string limits (UTF-16 units), while rejecting invalid UTF-8.
inline size_t textUnits(const char *text){
    const auto *p=reinterpret_cast<const uint8_t *>(text);const size_t n=strlen(text);
    if(!utf8(p,n))return SIZE_MAX;
    size_t count=0;
    for(size_t i=0;i<n;++i)if((p[i]&0xc0)!=0x80)count+=p[i]>=0xf0?2:1;
    return count;
}
inline int base64Digit(char c){
    if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;
}
inline bool bytes(const char *s,bool hex,uint8_t *out,size_t &length){
    if(!s)return false;const size_t n=strlen(s);length=0;
    if(hex){
        if(n!=64)return false;
        for(size_t i=0;i<n;i+=2){unsigned value=0;for(unsigned k=0;k<2;++k){char c=s[i+k];
            int d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(d<0)return false;value=value*16+d;}
            out[length++]=uint8_t(value);
        }return true;
    }
    if(n%4||n>344)return false;
    for(size_t i=0;i<n;i+=4){
        int a=base64Digit(s[i]),b=base64Digit(s[i+1]);if(a<0||b<0)return false;
        const bool pad2=s[i+2]=='=',pad3=s[i+3]=='=';
        int c=pad2?0:base64Digit(s[i+2]),d=pad3?0:base64Digit(s[i+3]);
        if(c<0||d<0||(pad2&&!pad3)||((pad2||pad3)&&i+4!=n)||
           (pad2&&(b&15))||(!pad2&&pad3&&(c&3)))return false;
        const size_t count=pad2?1:pad3?2:3;if(length+count>256)return false;
        out[length++]=uint8_t((a<<2)|(b>>4));
        if(!pad2)out[length++]=uint8_t((b<<4)|(c>>2));if(!pad3)out[length++]=uint8_t((c<<6)|d);
    }return true;
}
inline cJSON *byteString(const uint8_t *data,size_t size,bool hex){
    char result[345];size_t at=0;
    if(hex){if(size!=32)return nullptr;const char digits[]="0123456789abcdef";
        for(size_t i=0;i<size;++i){result[at++]=digits[data[i]>>4];result[at++]=digits[data[i]&15];}}
    else{if(size>256)return nullptr;const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for(size_t i=0;i<size;i+=3){uint32_t v=uint32_t(data[i])<<16;
            if(i+1<size)v|=uint32_t(data[i+1])<<8;if(i+2<size)v|=data[i+2];
            result[at++]=alphabet[(v>>18)&63];result[at++]=alphabet[(v>>12)&63];
            result[at++]=i+1<size?alphabet[(v>>6)&63]:'=';result[at++]=i+2<size?alphabet[v&63]:'=';}}
    result[at]=0;return cJSON_CreateString(result);
}
inline bool body(Writer &,uint8_t,const cJSON *,unsigned);
inline bool field(Writer &w,const Field &f,const cJSON *v,unsigned depth){
    const unsigned wt=f.type==1||f.type==6?1:f.type==7?5:f.type==9||f.type==12||f.type==11?2:0;
    if(!w.integer(uint64_t(f.id)*8+wt))return false;
    if(f.type==11){
        cJSON *parsed=cJSON_IsRaw(v)?cJSON_Parse(v->valuestring):nullptr;
        const cJSON *object=parsed?parsed:v;Writer size{nullptr,MvtWssWire::MaxFrame};
        const bool ok=body(size,f.sub,object,depth+1)&&w.integer(size.size)&&body(w,f.sub,object,depth+1);
        cJSON_Delete(parsed);return ok;
    }
    if(f.type==9||f.type==12){
        if(!cJSON_IsString(v)||!v->valuestring)return false;
        const auto *data=reinterpret_cast<const uint8_t *>(v->valuestring);size_t length=strlen(v->valuestring);
        uint8_t raw[256];
        if(f.type==12){if(!bytes(v->valuestring,!strcmp(f.name,"sha256"),raw,length))return false;data=raw;}
        else if(length>512||!utf8(data,length))return false;
        return w.integer(length)&&w.put(data,length);
    }
    if(f.type==8){
        if(cJSON_IsBool(v))return w.integer(cJSON_IsTrue(v)?1:0);
        // Existing diagnostic state stores duringSession as an integer.
        if(!strcmp(f.name,"duringSession")&&cJSON_IsNumber(v)&&(v->valuedouble==0||v->valuedouble==1))return w.integer(uint64_t(v->valuedouble));
        return false;
    }
    if(!cJSON_IsNumber(v)||!std::isfinite(v->valuedouble))return false;
    const double d=v->valuedouble;
    if(f.type==1){uint64_t bits;memcpy(&bits,&d,8);uint8_t b[8];for(unsigned i=0;i<8;++i)b[i]=uint8_t(bits>>(i*8));return w.put(b,8);}
    if(std::floor(d)!=d)return false;
    if(f.type==17){if(d<INT32_MIN||d>INT32_MAX)return false;const int64_t s=int64_t(d);return w.integer(s<0?uint64_t(-s*2-1):uint64_t(s*2));}
    if(d<0||d>double(f.type==7||f.type==13?UINT32_MAX:MvtWssWire::MaxInteger))return false;
    const uint64_t value=uint64_t(d);
    if(f.type==7||f.type==6){uint8_t b[8];unsigned n=f.type==7?4:8;for(unsigned i=0;i<n;++i)b[i]=uint8_t(value>>(i*8));return w.put(b,n);}
    return (f.type==4||f.type==13)&&w.integer(value);
}
inline bool body(Writer &w,uint8_t index,const cJSON *object,unsigned depth){
    if(depth>6||!cJSON_IsObject(object))return false;
    const Message &message=messages[index];
    for(size_t i=0;i<message.count;++i){const Field &f=message.fields[i];
        const cJSON *value=cJSON_GetObjectItemCaseSensitive(object,f.name);if(!value)continue;
        // Node's optional null values explicitly clear previous measurements.
        if(cJSON_IsNull(value)&&f.optional){
            bool node=false;for(size_t k=0;k<message.count;++k)if(!strcmp(message.fields[k].name,"clearedFields"))node=true;
            if(!node||!w.integer(39*8)||!w.integer(f.id))return false;continue;
        }
        if(f.repeated){if(!cJSON_IsArray(value)||cJSON_GetArraySize(value)>32)return false;
            for(const cJSON *item=value->child;item;item=item->next)if(!field(w,f,item,depth))return false;
        }else if(!field(w,f,value,depth))return false;
    }return true;
}
inline uint8_t kind(const char *type){if(type)for(unsigned i=1;i<=MvtWssWire::MaxKind;++i)if(!strcmp(type,kinds[i]))return uint8_t(i);return 0;}
inline size_t encode(uint8_t *out,size_t capacity,uint8_t type,const cJSON *object){
    if(!type||type>MvtWssWire::MaxKind||capacity<8)return 0;
    if(capacity>MvtWssWire::MaxFrame)capacity=MvtWssWire::MaxFrame;
    Writer w{out?out+8:nullptr,capacity-8};
    if(!body(w,bodies[type],object,0))return 0;
    if(!out)return w.size+8; // maximum envelope prefix, safe allocation bound
    return MvtWssWire::encode(out,capacity,type,out+8,w.size);
}
inline cJSON *decodeBody(uint8_t index,const uint8_t *data,size_t size,unsigned depth,unsigned &budget){
    if(depth>6||!budget)return nullptr;--budget;
    cJSON *out=cJSON_CreateObject();if(!out)return nullptr;
    MvtWssWire::Reader r(data,size);uint64_t seen=0;
    const Message &message=messages[index];
    while(r.offset<r.size){
        uint64_t tag=0,value=0;if(!budget||!r.integer(tag))goto fail;--budget;
        const Field *f=nullptr;for(size_t i=0;i<message.count;++i)if(message.fields[i].id==tag/8)f=&message.fields[i];
        if(!f||f->id>=64||(!f->repeated&&(seen&(uint64_t(1)<<f->id))))goto fail;
        seen|=uint64_t(1)<<f->id;
        cJSON *item=nullptr;const unsigned wt=unsigned(tag&7);
        if(f->type==4||f->type==13||f->type==17||f->type==8){
            if(wt||!r.integer(value)||(f->type!=4&&value>UINT32_MAX)||(f->type==8&&value>1))goto fail;
            if(f->type==8)item=cJSON_CreateBool(value!=0);
            else item=cJSON_CreateNumber(f->type==17?(value&1?-double((value+1)/2):double(value/2)):double(value));
        }else if(f->type==1||f->type==6||f->type==7){
            const unsigned n=f->type==7?4:8;if(wt!=(n==4?5:1)||n>r.size-r.offset)goto fail;
            for(unsigned i=0;i<n;++i)value|=uint64_t(r.data[r.offset++])<<(i*8);
            double d=double(value);if(f->type==1)memcpy(&d,&value,8);
            if(!std::isfinite(d)||(f->type==6&&value>MvtWssWire::MaxInteger))goto fail;
            item=cJSON_CreateNumber(d);
        }else if(f->type==9||f->type==12||f->type==11){
            if(wt!=2||!r.integer(value)||value>r.size-r.offset)goto fail;
            const uint8_t *p=r.data+r.offset;const size_t n=size_t(value);r.offset+=n;
            if(f->type==11)item=decodeBody(f->sub,p,n,depth+1,budget);
            else if(f->type==12)item=byteString(p,n,!strcmp(f->name,"sha256"));
            else{if(n>512||!utf8(p,n))goto fail;char text[513];memcpy(text,p,n);text[n]=0;item=cJSON_CreateString(text);}
        }else goto fail;
        if(!item)goto fail;
        if(f->repeated){
            cJSON *array=cJSON_GetObjectItemCaseSensitive(out,f->name);
            if(!array){array=cJSON_CreateArray();if(!array||!cJSON_AddItemToObject(out,f->name,array)){cJSON_Delete(array);cJSON_Delete(item);goto fail;}}
            if(cJSON_GetArraySize(array)>=32||!cJSON_AddItemToArray(array,item)){cJSON_Delete(item);goto fail;}
        }else if(!cJSON_AddItemToObject(out,f->name,item)){cJSON_Delete(item);goto fail;}
    }
    if(auto *clear=cJSON_GetObjectItemCaseSensitive(out,"clearedFields")){
        uint64_t cleared=0;
        for(auto *item=clear->child;item;item=item->next){
            const unsigned id=unsigned(item->valuedouble);const Field *f=nullptr;
            for(size_t i=0;i<message.count;++i)if(message.fields[i].id==id)f=&message.fields[i];
            if(!f||!f->optional||id>=64||(cleared&(uint64_t(1)<<id))||
               cJSON_GetObjectItemCaseSensitive(out,f->name))goto fail;
            cleared|=uint64_t(1)<<id;if(!cJSON_AddNullToObject(out,f->name))goto fail;
        }
        cJSON_DeleteItemFromObjectCaseSensitive(out,"clearedFields");
    }
    return out;
fail:cJSON_Delete(out);return nullptr;
}
inline cJSON *decode(const uint8_t *data,size_t size){
    MvtWssWire::View view;if(!MvtWssWire::decode(data,size,view))return nullptr;
    unsigned budget=256;cJSON *out=decodeBody(bodies[view.kind],view.payload,view.size,0,budget);
    if(out&&!cJSON_AddStringToObject(out,"type",kinds[view.kind])){cJSON_Delete(out);return nullptr;}
    // Existing handler consumes timestamp, while the keepalive schema names units.
    if(out&&(view.kind==4||view.kind==5)){auto *value=cJSON_DetachItemFromObjectCaseSensitive(out,"timestamp_ms");
        if(value&&!cJSON_AddItemToObject(out,"timestamp",value)){cJSON_Delete(value);cJSON_Delete(out);return nullptr;}}
    return out;
}
}
