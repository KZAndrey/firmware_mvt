// Host regression for the production transport lifetime critical section.
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

enum { WEBSOCKET_STATE_UNKNOW, WEBSOCKET_STATE_CONNECTED, WEBSOCKET_STATE_WAIT_TIMEOUT,
       WEBSOCKET_STATE_CLOSING, CLOSE_FRAME_SENT_BIT = 8, WEBSOCKET_EVENT_CLOSED = 9 };
const int portMAX_DELAY = -1;
#define ESP_LOGE(...) ((void)0)
thread_local int lockDepth=0;
std::atomic<unsigned> schedulerBlocks{0};
void vTaskDelay(int ticks) {
    assert(ticks>0&&lockDepth==0); // lower-priority sender must run outside lock
    ++schedulerBlocks;
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
struct Client {
    std::recursive_mutex *lock;
    bool run = true;
    int state = WEBSOCKET_STATE_CONNECTED;
    int transport = 1, status_bits = CLOSE_FRAME_SENT_BIT;
};
using esp_websocket_client_handle_t = Client *;
void xSemaphoreTakeRecursive(std::recursive_mutex *m, int) { m->lock();++lockDepth; }
void xSemaphoreGiveRecursive(std::recursive_mutex *m) { --lockDepth;m->unlock(); }
int xEventGroupGetBits(int bits) { return bits; }
std::mutex gate;
std::condition_variable changed;
bool entered = false, release_poll = false, block_poll = false;
std::atomic<int> polls{0}, closes{0}, active{0}, closed_events{0};
int poll_result = -1, close_result = 0;
int esp_transport_poll_read(int, int timeout) {
    assert(timeout <= 20);
    assert(++active == 1);
    ++polls;
    if (block_poll) {
        std::unique_lock<std::mutex> g(gate);
        entered = true;
        changed.notify_all();
        changed.wait(g, [] { return release_poll; });
    }
    --active;
    return poll_result;
}
int esp_transport_ws_poll_connection_closed(int, int timeout) {
    assert(timeout <= 20);
    return close_result;
}
void esp_websocket_client_abort_connection(Client *c) {
    assert(active == 0); // no free while another task polls TLS
    ++closes;
    c->state = WEBSOCKET_STATE_WAIT_TIMEOUT;
}
void esp_websocket_client_dispatch_event(Client *, int, void *, int) { ++closed_events; }
#include "../../src/platform/esp32/mvt_websocket_poll.inc"

int main() {
    std::recursive_mutex lock;
    Client c{&lock};
    int readable = 0;
    block_poll = true;
    std::thread receiver([&] { assert(mvt_websocket_poll(&c, &readable)); });
    {
        std::unique_lock<std::mutex> g(gate);
        assert(changed.wait_for(g, std::chrono::seconds(5), [] { return entered; }));
    }
    std::atomic<bool> sender_started{false}, sender_done{false};
    std::thread sender([&] {
        sender_started = true;
        std::lock_guard<std::recursive_mutex> g(lock);
        if (c.state == WEBSOCKET_STATE_CONNECTED) esp_websocket_client_abort_connection(&c);
        sender_done = true;
    });
    while (!sender_started) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    assert(!sender_done && closes == 0);
    { std::lock_guard<std::mutex> g(gate); release_poll = true; }
    changed.notify_all();
    receiver.join(); sender.join();
    assert(closes == 1 && c.state == WEBSOCKET_STATE_WAIT_TIMEOUT);
    assert(mvt_websocket_poll(&c, &readable) && polls == 1); // no stale poll after send abort
    block_poll = false;
    c.state = WEBSOCKET_STATE_CONNECTED; poll_result = 1;
    assert(mvt_websocket_poll(&c, &readable) && readable == 1 && closes == 1);
    c.state = WEBSOCKET_STATE_CLOSING;
    assert(mvt_websocket_poll(&c, &readable) && c.run && closed_events == 0);
    close_result = 1;
    assert(!mvt_websocket_poll(&c, &readable) && !c.run && closed_events == 1);
    assert(lock.try_lock()); lock.unlock(); // every close path releases the mutex
    c.run = false; c.state = WEBSOCKET_STATE_CONNECTED;
    assert(mvt_websocket_poll(&c, &readable) && polls == 2);
    assert(schedulerBlocks>=6);
    puts("WSS concurrent failure, close lifecycle and lower-priority sender scheduling: PASS");
}
