#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
enum {WS_TRANSPORT_OPCODES_PING=9,WS_TRANSPORT_OPCODES_PONG=10,WS_TRANSPORT_OPCODES_FIN=128};
struct Config{int network_timeout_ms=1000;} config;
struct Client{int control_transport=1;Config *config=&::config;};
using esp_websocket_client_handle_t=Client*;
uint32_t now=0;uint32_t _tick_get_ms(){return now;}
void esp_fill_random(void *p,size_t n){memset(p,0x53,n);}
std::string stage;void mvt_ws_diagnostic(Client*,const char *s,int,int){stage=s;}
int result=999,writeCount=0;std::vector<unsigned char> wire;
int esp_transport_write(int,const char *p,int n,int timeout){
 assert(timeout>0&&timeout<=1000);++writeCount;now+=10;
 int count=result==999?n:result;
 if(count>0)wire.insert(wire.end(),p,p+count);
 return count;
}
#include "../../src/platform/esp32/mvt_websocket_control.inc"
int main(){Client c;
 assert(mvt_websocket_send_control(&c,9,nullptr,0));assert(wire.size()==6&&wire[0]==0x89&&wire[1]==0x80);
 for(int error:{0,-1}){result=error;stage.clear();assert(!mvt_websocket_send_control(&c,10,nullptr,0));assert(stage=="pong_write");}
 result=2;wire.clear();assert(mvt_websocket_send_control(&c,10,"ab",2));assert(wire.size()==8&&wire[6]==('a'^0x53)&&wire[7]==('b'^0x53));
 result=999;wire.clear();std::string payload(125,'x');assert(mvt_websocket_send_control(&c,10,payload.data(),125));assert(wire.size()==131);
 assert(!mvt_websocket_send_control(&c,10,payload.data(),126));
 puts("PASS control frames: empty success, timeout/error, partial writes, mask and 125-byte limit");
}
