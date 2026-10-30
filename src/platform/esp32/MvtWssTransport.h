#pragma once
#if HAS_MVT_WSS_GATEWAY
#include <atomic>
#include <string>
#include "esp_websocket_client.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

// The mesh loop only submits bounded messages and requests lifecycle changes.
// This worker exclusively owns the SDK client; TLS waits never run on the mesh loop.
class MvtWssTransport {
public:
    using Callback = void (*)(void *, esp_event_base_t, int32_t, void *);
    MvtWssTransport(Callback callback, void *context) : callback(callback), context(context) {}
    ~MvtWssTransport();
    bool start(const std::string &url, const char *certificate, uint32_t epoch);
    void stop();
    void shutdown(); // joins only during destruction, never from the mesh loop
    bool active() const { return state.load() != Idle; }
    bool connected() const { return online.load() && !stopping.load(); }
    bool send(const std::string &text, bool priority, bool replay);
    bool takeFailed(std::string &text, uint32_t &epoch);
private:
    enum State { Idle, Starting, Running };
    struct Entry { size_t size; uint32_t epoch; bool replay; char text[1]; };
    static constexpr unsigned MAX_MESSAGES = 8;
    static constexpr size_t MAX_BYTES = 16384;
    static constexpr EventBits_t EXITED = 1;
    Callback callback;
    void *context;
    QueueHandle_t urgent = nullptr, pending = nullptr, failed = nullptr;
    EventGroupHandle_t finished = nullptr;
    TaskHandle_t worker = nullptr;
    SemaphoreHandle_t admission = nullptr;
    esp_websocket_client_handle_t client = nullptr;
    std::atomic<State> state{Idle};
    std::atomic<bool> online{false}, stopping{false}, exiting{false};
    std::atomic<unsigned> messages{0};
    std::atomic<size_t> bytes{0};
    std::string url;
    const char *certificate = nullptr;
    uint32_t epoch = 0;
    bool ensureWorker();
    static void task(void *);
    static void event(void *, esp_event_base_t, int32_t, void *);
    void run();
    void release(Entry *);
    void undelivered(Entry *);
    void drain();
};
#endif
