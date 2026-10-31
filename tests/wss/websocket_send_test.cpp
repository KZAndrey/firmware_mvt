// Execute the production sender with a transport that distinguishes a header
// already on the wire from a timeout before any write. No network/hardware.
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
using TickType_t = uint32_t;
using ws_transport_opcodes_t = int;
constexpr int ESP_FAIL=-1, pdPASS=1, WS_TRANSPORT_OPCODES_TEXT=1,
    WS_TRANSPORT_OPCODES_CLOSE=8, WS_TRANSPORT_OPCODES_FIN=128;
constexpr TickType_t portMAX_DELAY=UINT32_MAX, portTICK_PERIOD_MS=1;
#define ESP_LOGE(...) ((void)0)
struct Client {
    std::recursive_timed_mutex *lock;
    bool connected=true; int transport=1, control_transport=2, buffer_size=4;
    char tx_buffer[4];
};
using esp_websocket_client_handle_t=Client*;
uint32_t now=0; uint32_t _tick_get_ms(){return now;}
int xSemaphoreTakeRecursive(std::recursive_timed_mutex *m, TickType_t timeout){
    if(timeout==portMAX_DELAY){m->lock();return pdPASS;}
    return m->try_lock_for(std::chrono::milliseconds(timeout))?pdPASS:0;
}
void xSemaphoreGiveRecursive(std::recursive_timed_mutex *m){m->unlock();}
bool esp_websocket_client_is_connected(Client *c){return c->connected;}
int aborts=0, rawCalls=0, headerBytes=0, polls=0;
int busyPolls=0, pollError=0, rawError=0, rawResult=999, stallAfterFragment=-1, delayMs=0;
std::string stage, delivered; int savedError=0;
std::vector<int> opcodes, timeouts;
void mvt_ws_diagnostic(Client*,const char *s,int,int e){stage=s;savedError=e;}
void esp_websocket_client_abort_connection(Client *c){++aborts;c->connected=false;}
int esp_transport_poll_write(int,int timeout){
    ++polls; assert(timeout>0 || timeout==-1);
    if(pollError){errno=pollError;return -1;}
    if(busyPolls>0 || rawCalls==stallAfterFragment){--busyPolls;now+=timeout;return 0;}
    if(delayMs){const int spent=timeout<0?delayMs:std::min(delayMs,timeout);now+=spent;delayMs-=spent;return delayMs?0:1;}
    return 1;
}
int esp_transport_ws_send_raw(int,int opcode,char *data,int len,int timeout){
    // Match IDF: raw send polls internally, then writes header and payload.
    const int ready=esp_transport_poll_write(2,timeout);
    ++rawCalls;opcodes.push_back(opcode);timeouts.push_back(timeout);
    if(ready<=0)return ready;
    headerBytes+=6; // Header succeeded even if payload returns zero/error.
    if(rawResult!=999){errno=rawError;return rawResult;}
    delivered.append(data,len);return len;
}
#include "../../src/platform/esp32/mvt_websocket_send.inc"
int main(int argc,char **argv){
    const std::string scenario=argc>1?argv[1]:"all";
    std::recursive_timed_mutex mutex; Client c{&mutex};
    auto reset=[&]{c.connected=true;aborts=rawCalls=headerBytes=polls=0;busyPolls=pollError=rawError=0;
        rawResult=999;stallAfterFragment=-1;delayMs=0;stage.clear();delivered.clear();opcodes.clear();timeouts.clear();now=0;};
    auto send=[&](const char *s,TickType_t timeout=5000){return esp_websocket_client_send_with_opcode(&c,1,(const uint8_t*)s,int(strlen(s)),timeout);};
    if(scenario=="all"||scenario=="prewrite"){
        reset();busyPolls=1;errno=EAGAIN;
        assert(send("abc")==ESP_FAIL);assert(c.connected&&aborts==0&&rawCalls==0&&headerBytes==0);
        assert(send("abc")==3&&delivered=="abc"); // exactly once, same session
    }
    if(scenario=="all"||scenario=="partial"){
        for(int result:{0,-1,2,5}){reset();rawResult=result;rawError=EAGAIN;
            assert(send("abcd")!=4);assert(aborts==1&&!c.connected&&headerBytes==6);
            assert(stage=="write"&&savedError==EAGAIN);
            assert(send("next")==ESP_FAIL&&rawCalls==1); // never append a retry
        }
    }
    if(scenario=="all"||scenario=="lock"){
        reset();std::atomic<bool> reading{false},release{false};
        std::thread receiver([&]{std::lock_guard<std::recursive_timed_mutex> guard(mutex);reading=true;
            while(!release)std::this_thread::yield();});
        while(!reading)std::this_thread::yield();
        assert(send("abc",20)==ESP_FAIL);assert(c.connected&&aborts==0&&rawCalls==0);
        release=true;receiver.join();assert(send("abc")==3&&delivered=="abc");
    }
    if(scenario=="all"||scenario=="fragment"){
        reset();assert(send("abcdefghi")==9&&delivered=="abcdefghi");
        assert((opcodes==std::vector<int>{1,0,128}));
        reset();stallAfterFragment=1;assert(send("abcdefghi")!=9);
        assert(aborts==1&&!c.connected&&delivered=="abcd");
    }
    if(scenario=="all"||scenario=="poll"){
        for(int error:{EAGAIN,EINTR,ECONNRESET}){reset();pollError=error;
            assert(send("abc")==ESP_FAIL&&rawCalls==0);
            assert(c.connected==(error!=ECONNRESET));assert(aborts==(error==ECONNRESET?1:0));
        }
        reset();errno=EAGAIN;rawResult=0;rawError=0;
        assert(send("abc")!=3&&aborts==1&&savedError==0); // no stale errno
        reset();now=UINT32_MAX-10;assert(send("abc")==3);
        reset();assert(send("abc",0)==ESP_FAIL&&rawCalls==0&&c.connected);
        reset();delayMs=1500;assert(send("abc",5000)==3&&aborts==0);
        assert(timeouts.front()==3500); // preflight consumes the send budget
        reset();delayMs=1500;assert(send("abc",1000)==ESP_FAIL&&c.connected&&rawCalls==0);
        assert(send("abc",5000)==3&&delivered=="abc");
    }
    assert(mutex.try_lock());mutex.unlock();
    printf("PASS production send: %s\n",scenario.c_str());
}
