#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <string>
using esp_err_t = int;
enum { ESP_OK, ESP_FAIL, WS_TRANSPORT_OPCODES_NONE, WS_TRANSPORT_OPCODES_PING,
       WS_TRANSPORT_OPCODES_PONG, WS_TRANSPORT_OPCODES_CLOSE, WS_TRANSPORT_OPCODES_FIN=128,
       WEBSOCKET_EVENT_DATA, WEBSOCKET_STATE_CLOSING };
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGV(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
struct Config { int network_timeout_ms=10000; } config;
struct Client { bool run=true, wait_for_pong_resp=true; int payload_offset=0,payload_len=0,last_opcode=0;
    int transport=0,buffer_size=4,state=0; char rx_buffer[4]; Config *config=&::config; };
using esp_websocket_client_handle_t=Client*;
std::string diagnosticStage;int diagnosticErrno=0;
void mvt_ws_diagnostic(Client*,const char* stage,int,int error) {diagnosticStage=stage;diagnosticErrno=error;}
uint32_t tick=0; int calls=0, events=0, mode=0;
Client *active;
uint32_t _tick_get_ms(){return tick;}
int esp_transport_read(int,char*,int,int){
    assert(++calls<20); // old receive loop never exits on a stalled frame
    tick+=10000;
    if(mode==7){errno=ECONNRESET;return -1;}
    if(mode==2) active->run=false;
    return mode==1||mode==4||mode==5||(mode==6&&calls>1)?0:4;
}
int esp_transport_ws_get_read_payload_len(int){return mode==3?1000:mode==4?0:8;}
int esp_transport_ws_get_read_opcode(int){return mode==5||(mode==6&&calls>1)?WS_TRANSPORT_OPCODES_NONE:mode==4?WS_TRANSPORT_OPCODES_PING:42;}
void esp_websocket_client_dispatch_event(Client*,int,char*,int){++events;}
bool controlSuccess=true;
bool mvt_websocket_send_control(Client*,int,const char*,int){return controlSuccess;}
#include "../../src/platform/esp32/mvt_websocket_receive.inc"
int main(){
    for(mode=0;mode<8;++mode){Client c;active=&c;tick=0;calls=events=0;diagnosticStage.clear();diagnosticErrno=0;
        int result=esp_websocket_client_recv(&c);
        if(mode==0){assert(result==ESP_OK&&calls==2&&events==2);}
        else if(mode==4||mode==5){assert(result==ESP_OK&&calls==1);}
        else {assert(result==ESP_FAIL&&calls<=3);}
        if(mode==7){assert(diagnosticStage=="read"&&diagnosticErrno==ECONNRESET);}
        if(mode==1)assert(diagnosticStage=="partial_frame_stalled");
    }
    Client c;active=&c;mode=4;tick=0;calls=events=0;controlSuccess=false;
    assert(esp_websocket_client_recv(&c)==ESP_FAIL); // failed PONG retires socket
    puts("PASS receive: chunks, no progress, stop, frame deadline, empty ping, idle and partial timeouts");
}
