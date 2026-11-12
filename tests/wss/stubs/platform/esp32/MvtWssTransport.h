#pragma once
#include "esp_websocket_client.h"
extern bool testAdmissionFull;
// Deterministic adapter for gateway protocol tests. The real asynchronous
// worker is exercised separately by transport_worker_test.cpp.
class MvtWssTransport {
public:
    using Callback=void(*)(void*,esp_event_base_t,int32_t,void*);
    void *client=nullptr;
    MvtWssTransport(Callback,void*) {}
    bool start(const std::string &,const char *,uint32_t) {
        client=esp_websocket_client_init(nullptr);
        if(client&&esp_websocket_client_start(client)!=ESP_OK){stop();return false;}
        return client;
    }
    void stop(){if(client){esp_websocket_client_destroy(client);client=nullptr;}}
    void shutdown(){stop();}
    bool active() const{return client;}
    bool connected() const{return client&&!testSendFailure;}
    bool canSend(bool) const{return connected()&&!testAdmissionFull;}
    bool send(const std::string &s,bool,bool){return client&&esp_websocket_client_send_text(client,s.c_str(),s.size(),1000)==int(s.size());}
    bool send(const char *s,size_t length,bool,bool){return client&&esp_websocket_client_send_text(client,s,length,1000)==int(length);}
    bool sendOwned(char *s,size_t length,void (*dispose)(void *),bool priority,bool replay){
        if(!dispose||!send(s,length,priority,replay))return false;
        dispose(s);return true;
    }
    bool takeFailed(std::string &,uint32_t &){return false;}
};
