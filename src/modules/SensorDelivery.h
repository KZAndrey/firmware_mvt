#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Fixed RAM budget, three transmissions maximum, no retry after thirty minutes.
// millis subtraction deliberately supports wraparound.
namespace SensorDelivery {
constexpr uint32_t RETRY_MS = 600000, TTL_MS = 1800000;
constexpr size_t CAPACITY = 4, MAX_PAYLOAD = 71;
inline void put32(uint8_t *p, uint32_t v) { for (int i=0;i<4;i++) p[i]=v>>(8*i); }
inline uint32_t get32(const uint8_t *p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
struct Item {
    uint8_t bytes[MAX_PAYLOAD] = {}, size = 0, attempts = 0;
    uint32_t born = 0, last = 0;
};
class Queue {
  public:
    Item items[CAPACITY];
    uint16_t dropped = 0;
    void clear() { for(auto &i:items)i.size=0; }
    Item &allocate(uint32_t now) {
        for(auto &i:items)if(!i.size){i=Item{};i.born=now;return i;}
        size_t oldest=0;
        for(size_t n=1;n<CAPACITY;n++)if(uint32_t(now-items[n].born)>uint32_t(now-items[oldest].born))oldest=n;
        if(dropped!=65535)++dropped;
        items[oldest]=Item{};items[oldest].born=now;return items[oldest];
    }
    Item *due(uint32_t now) {
        for(auto &i:items){
            if(!i.size)continue;
            if(uint32_t(now-i.born)>=TTL_MS){i.size=0;if(dropped!=65535)++dropped;continue;}
            if(i.attempts<3 && (!i.attempts||uint32_t(now-i.last)>=RETRY_MS))return &i;
        }
        return nullptr;
    }
    bool acknowledge(const uint8_t *ack) {
        for(auto &i:items)if(i.size && !memcmp(i.bytes+9,ack+13,8) && get32(i.bytes+17)==get32(ack+21)
            && !memcmp(i.bytes+i.size-16,ack+25,16)){i.size=0;return true;}
        return false;
    }
};
}
