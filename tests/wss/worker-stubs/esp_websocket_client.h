#pragma once
#include "freertos/FreeRTOS.h"
using esp_event_base_t=void*;
constexpr int ESP_OK=0,WEBSOCKET_EVENT_ANY=-1,WEBSOCKET_EVENT_CONNECTED=1,WEBSOCKET_EVENT_DISCONNECTED=2,WEBSOCKET_EVENT_CLOSED=4;
struct esp_websocket_client_config_t {const char *uri,*cert_pem;int task_stack,buffer_size,ping_interval_sec,pingpong_timeout_sec;bool disable_auto_reconnect;};
struct FakeClient {void(*event)(void*,void*,int32_t,void*);void *arg;};
using esp_websocket_client_handle_t=FakeClient*;
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t*);
int esp_websocket_register_events(FakeClient*,int,void(*)(void*,void*,int32_t,void*),void*);
int esp_websocket_client_start(FakeClient*);
int esp_websocket_client_stop(FakeClient*);
int esp_websocket_client_destroy(FakeClient*);
int esp_websocket_client_send_text(FakeClient*,const char*,int,int);
bool esp_websocket_client_is_connected(FakeClient*);
