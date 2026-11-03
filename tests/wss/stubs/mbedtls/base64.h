#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
// Host codec adapter. ESP32 builds link the platform's mbedTLS implementation.
inline int mbedtls_base64_encode(uint8_t *out,size_t capacity,size_t *written,const uint8_t *in,size_t size) {
    const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    *written=((size+2)/3)*4; if(capacity<*written+1)return -1;
    size_t at=0;
    for(size_t i=0;i<size;i+=3){
        uint32_t n=uint32_t(in[i])<<16;
        if(i+1<size)n|=uint32_t(in[i+1])<<8;
        if(i+2<size)n|=in[i+2];
        out[at++]=alphabet[(n>>18)&63];out[at++]=alphabet[(n>>12)&63];
        out[at++]=i+1<size?alphabet[(n>>6)&63]:'=';out[at++]=i+2<size?alphabet[n&63]:'=';
    }
    out[at]=0;return 0;
}
inline int mbedtls_base64_decode(uint8_t *out,size_t capacity,size_t *written,const uint8_t *in,size_t size) {
    const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint32_t value=0;int bits=0;*written=0;
    if(size%4)return -1;
    for(size_t i=0;i<size;++i){
        if(in[i]=='=')break;
        const char *p=strchr(alphabet,in[i]);if(!p||!in[i])return -1;
        value=(value<<6)|uint32_t(p-alphabet);bits+=6;
        if(bits>=8){bits-=8;if(*written>=capacity)return -1;out[(*written)++]=uint8_t(value>>bits);}
    }
    return 0;
}
