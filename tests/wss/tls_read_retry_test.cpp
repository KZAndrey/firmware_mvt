#include <cassert>
#include <cstdio>
#include <vector>
#include "../../src/platform/esp32/MvtTlsReadRetry.h"
int main(){
 const int want=-0x6900; unsigned retries=0; uint32_t tick=0; int calls=0;
 // Same caller buffer/length: a transient result must not escape to the WS header parser.
 std::vector<int> sequence{want,want,2};
 int r=mvtTlsReadRetry(10000,[&](int remaining){assert(remaining>0&&remaining<=10000);++calls;return sequence[calls-1];},[&]{return tick;},[&]{tick+=10;},retries);
 assert(r==2&&calls==3&&retries==2);
 tick=0;calls=0;r=mvtTlsReadRetry(30,[&](int){++calls;return want;},[&]{return tick;},[&]{tick+=10;},retries);
 assert(r==want&&calls==3&&tick==30);
 calls=0;r=mvtTlsReadRetry(0,[&](int t){assert(t==0);++calls;return want;},[&]{return tick;},[&]{assert(false);},retries);assert(r==want&&calls==1);
 for(int result:{0,-1,-0x7880,5}){calls=0;r=mvtTlsReadRetry(100,[&](int){++calls;return result;},[&]{return tick;},[&]{assert(false);},retries);assert(r==result&&calls==1);}
 tick=UINT32_MAX-10;calls=0;r=mvtTlsReadRetry(30,[&](int){++calls;return want;},[&]{return tick;},[&]{tick+=10;},retries);assert(r==want&&calls==3);
 tick=0;calls=0;r=mvtTlsReadRetry(100,[&](int){++calls;tick+=100;return want;},[&]{return tick;},[&]{assert(false);},retries);assert(r==want&&calls==1); // no extension of an exhausted network timeout
 tick=0;calls=0;r=mvtTlsReadRetry(-1,[&](int remaining){assert(remaining<=10000);++calls;return want;},[&]{return tick;},[&]{tick+=5000;},retries);assert(r==want&&calls==2&&tick==10000);
 // One successful header byte, then WANT_READ while getting the second byte.
 // The outer parser retains its first byte because the retry never escapes it.
 unsigned char header[2]={0};int wire=0;tick=0;
 for(int offset=0;offset<2;++offset){int n=mvtTlsReadRetry(100,[&](int){++wire;if(wire==2)return want;header[offset]=offset?126:130;return 1;},[&]{return tick;},[&]{++tick;},retries);assert(n==1);}
 assert(header[0]==130&&header[1]==126&&wire==3);
 puts("TLS read retry: transient recovery, unchanged parser buffer, finite deadline, zero timeout, fatal/EOF passthrough and clock wrap: PASS");
}
