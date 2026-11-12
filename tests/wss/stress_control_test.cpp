#include "../../src/modules/MvtWssStressControl.h"
#include <cassert>
#include <vector>
#include <cstdio>
std::vector<uint8_t> envelope(uint32_t value){
    std::vector<uint8_t> out{0x3a,0,8};
    do{uint8_t b=value&127;value>>=7;out.push_back(b|(value?128:0));}while(value);
    out[1]=out.size()-2;return out;
}
int main(){
    for(unsigned n=1;n<=18;++n){auto b=envelope(0x4d560000u|n);assert(mvtStressDecode(b.data(),b.size())==n);
        for(size_t size=0;size<b.size();++size)assert(!mvtStressDecode(b.data(),size));
        auto extra=b;extra.push_back(0);assert(!mvtStressDecode(extra.data(),extra.size()));
        b[0]=0x12;assert(!mvtStressDecode(b.data(),b.size()));}
    for(uint32_t n: {0u,1u,0xffffffffu,0x4d560000u,0x4d560013u,0x4d570002u}){
        auto b=envelope(n);assert(!mvtStressDecode(b.data(),b.size()));}
    auto overflow=envelope(0x4d560001);overflow.back()=0x74;assert(!mvtStressDecode(overflow.data(),overflow.size()));
    assert(!mvtStressDecode(nullptr,8));
    assert(mvtStressDuration(3)==5000&&mvtStressDuration(4)==30000&&mvtStressDuration(5)==120000&&mvtStressDuration(7)==60000);
    assert(mvtStressDuration(2)==0&&mvtStressDuration(6)==0);
    assert(mvtStressBefore(0xfffffff0,32));assert(!mvtStressBefore(32,32));assert(!mvtStressBefore(33,32));assert(!mvtStressBefore(10,0));
    puts("PASS USB stress protocol: strict envelope, bounded commands, finite durations and clock wrap");
}
