#pragma once
#include "esp_websocket_client.h"
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
    bool send(const std::string &s,bool,bool){return client&&esp_websocket_client_send_text(client,s.c_str(),s.size(),1000)==int(s.size());}
    bool takeFailed(std::string &,uint32_t &){return false;}
};
