#if HAS_MVT_WSS_GATEWAY
#include "MvtWssTransport.h"
#include <cstdlib>
#include <cstring>

bool MvtWssTransport::ensureWorker() {
    if (worker) return true;
    if (!urgent) urgent = xQueueCreate(2, sizeof(Entry *));
    if (!pending) pending = xQueueCreate(MAX_MESSAGES, sizeof(Entry *));
    if (!failed) failed = xQueueCreate(MAX_MESSAGES, sizeof(Entry *));
    if (!finished) finished = xEventGroupCreate();
    if (!admission) admission = xSemaphoreCreateMutex();
    if (!urgent || !pending || !failed || !finished || !admission) return false;
    return xTaskCreate(task, "mvt_wss_io", 6144, this, 1, &worker) == pdPASS;
}

bool MvtWssTransport::start(const std::string &address, const char *ca, uint32_t revision) {
    if (active() || !ensureWorker()) return false;
    url = address; certificate = ca; epoch = revision;
    stopping = false;
    state = Starting; // release publishes immutable configuration to worker
    return true;
}

void MvtWssTransport::stop() { stopping = true; online = false; }

bool MvtWssTransport::send(const std::string &text, bool priority, bool replay) {
    if (!connected() || state != Running || text.empty() || text.size() > 8192) return false;
    if (messages.fetch_add(1) >= MAX_MESSAGES) { --messages; return false; }
    if (bytes.fetch_add(text.size()) + text.size() > MAX_BYTES) {
        bytes.fetch_sub(text.size()); --messages; return false;
    }
    auto *entry = static_cast<Entry *>(malloc(sizeof(Entry) + text.size()));
    if (!entry) { bytes.fetch_sub(text.size()); --messages; return false; }
    entry->size = text.size(); entry->epoch = epoch; entry->replay = replay;
    memcpy(entry->text, text.c_str(), text.size() + 1);
    if (xSemaphoreTake(admission, 0) != pdTRUE) { release(entry); return false; }
    const bool accepted = connected() && state == Running &&
        xQueueSend(priority ? urgent : pending, &entry, 0) == pdTRUE;
    xSemaphoreGive(admission);
    if (!accepted) release(entry);
    return accepted;
}

void MvtWssTransport::release(Entry *entry) {
    bytes.fetch_sub(entry->size); --messages; free(entry);
}

void MvtWssTransport::undelivered(Entry *entry) {
    // All queues plus the in-flight entry share MAX_MESSAGES slots, so every
    // replayable entry fits in failed. No allocation or blocking on this path.
    if (!entry->replay || xQueueSend(failed, &entry, 0) != pdTRUE) release(entry);
}

bool MvtWssTransport::takeFailed(std::string &text, uint32_t &revision) {
    Entry *entry = nullptr;
    if (!failed || xQueueReceive(failed, &entry, 0) != pdTRUE) return false;
    text.assign(entry->text, entry->size); revision = entry->epoch; release(entry);
    return true;
}

void MvtWssTransport::drain() {
    Entry *entry;
    while (xQueueReceive(urgent, &entry, 0) == pdTRUE) undelivered(entry);
    while (xQueueReceive(pending, &entry, 0) == pdTRUE) undelivered(entry);
}

void MvtWssTransport::event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    auto *self = static_cast<MvtWssTransport *>(arg);
    if (self->stopping.load()) return;
    if (id == WEBSOCKET_EVENT_CONNECTED) self->online = true;
    if (id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_CLOSED) self->online = false;
    self->callback(self->context, base, id, data);
}

void MvtWssTransport::task(void *arg) { static_cast<MvtWssTransport *>(arg)->run(); }

void MvtWssTransport::run() {
    Entry *inFlight = nullptr;
    while (!exiting.load()) {
        if (state == Starting && !stopping.load()) {
            esp_websocket_client_config_t cfg = {};
            cfg.uri = url.c_str(); cfg.cert_pem = certificate;
            cfg.task_stack = 6144; cfg.buffer_size = 2048;
            cfg.ping_interval_sec = 20; cfg.pingpong_timeout_sec = 75;
            cfg.disable_auto_reconnect = true;
            client = esp_websocket_client_init(&cfg);
            if (client) esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, event, this);
            state = Running;
            if (!client || esp_websocket_client_start(client) != ESP_OK) {
                callback(context, nullptr, WEBSOCKET_EVENT_DISCONNECTED, nullptr);
                stopping = true;
            }
        }
        if (stopping.load() && state != Idle) {
            online = false;
            if (client) {
                esp_websocket_client_stop(client);
                esp_websocket_client_destroy(client); client = nullptr;
            }
            online = false; // receiver has joined; no old callback can revive it
            if (inFlight) { undelivered(inFlight); inFlight = nullptr; }
            xSemaphoreTake(admission, portMAX_DELAY);
            drain();
            state = Idle; // receiver has joined; caller may now release its frame
            xSemaphoreGive(admission);
        }
        if (connected() && state == Running) {
            if (!inFlight && xQueueReceive(urgent, &inFlight, 0) != pdTRUE)
                xQueueReceive(pending, &inFlight, 0);
            if (inFlight) {
                const int sent = esp_websocket_client_send_text(client, inFlight->text, inFlight->size, pdMS_TO_TICKS(5000));
                if (sent == int(inFlight->size)) { release(inFlight); inFlight = nullptr; }
                else if (!(sent < 0 && esp_websocket_client_is_connected(client))) {
                    // Do not append another frame after a short/failed write.
                    callback(context, nullptr, WEBSOCKET_EVENT_DISCONNECTED, nullptr);
                    stopping = true; online = false;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    online = false;
    if (client) { esp_websocket_client_stop(client); esp_websocket_client_destroy(client); client = nullptr; }
    if (inFlight) release(inFlight);
    drain(); state = Idle;
    xEventGroupSetBits(finished, EXITED);
    vTaskDelete(nullptr);
}

void MvtWssTransport::shutdown() {
    if (worker) { stopping = true; exiting = true; xEventGroupWaitBits(finished, EXITED, pdFALSE, pdTRUE, portMAX_DELAY); worker = nullptr; }
}

MvtWssTransport::~MvtWssTransport() {
    shutdown();
    Entry *entry;
    if (failed) { while (xQueueReceive(failed, &entry, 0) == pdTRUE) release(entry); vQueueDelete(failed); }
    if (urgent) vQueueDelete(urgent);
    if (pending) vQueueDelete(pending);
    if (finished) vEventGroupDelete(finished);
    if (admission) vSemaphoreDelete(admission);
}
#endif
