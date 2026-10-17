#pragma once
#include "configuration.h"
#include "freertos/FreeRTOS.h"
typedef void *esp_websocket_client_handle_t;
typedef void *esp_event_base_t;
#define ESP_OK 0
#define WEBSOCKET_EVENT_ANY -1
#define WEBSOCKET_EVENT_CONNECTED 1
#define WEBSOCKET_EVENT_DISCONNECTED 2
#define WEBSOCKET_EVENT_DATA 3
#define WEBSOCKET_EVENT_CLOSED 4
struct esp_websocket_event_data_t {
    const char *data_ptr; int data_len; uint8_t op_code; void *client; void *user_context;
    int payload_len; int payload_offset;
};
struct esp_websocket_client_config_t {
    const char *uri; const char *cert_pem; int task_stack, buffer_size, ping_interval_sec, pingpong_timeout_sec;
    bool disable_auto_reconnect;
};
extern std::vector<std::string> testSent;
extern bool testSendFailure;
extern bool testSendBusy;
inline bool esp_websocket_client_is_connected(void *) { return !testSendFailure; }
inline void *esp_websocket_client_init(const esp_websocket_client_config_t *) { return reinterpret_cast<void *>(1); }
inline int esp_websocket_client_start(void *) { return ESP_OK; }
inline int esp_websocket_client_stop(void *) { return ESP_OK; }
inline int esp_websocket_client_destroy(void *) { return ESP_OK; }
inline int esp_websocket_register_events(void *, int, void (*)(void *, void *, int32_t, void *), void *) { return ESP_OK; }
inline int esp_websocket_client_send_text(void *, const char *p, int len, int) {
    if (testSendFailure || testSendBusy) return -1;
    testSent.emplace_back(p, len); return len;
}
