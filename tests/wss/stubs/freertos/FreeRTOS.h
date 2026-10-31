#pragma once
#include <deque>
#include <cstdint>
#include <cstring>
typedef int TickType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(n) (n)
struct TestQueue { size_t max; std::deque<void *> values; };
typedef TestQueue *QueueHandle_t;
inline QueueHandle_t xQueueCreate(int max, size_t) { return new TestQueue{size_t(max), {}}; }
inline int xQueueSend(QueueHandle_t q, const void *v, int) {
    if (q->values.size() >= q->max) return 0;
    void *value; memcpy(&value, v, sizeof(value));
    q->values.push_back(value); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void *v, int) {
    if (q->values.empty()) return 0;
    memcpy(v, &q->values.front(), sizeof(void *)); q->values.pop_front(); return pdTRUE;
}
inline void vQueueDelete(QueueHandle_t q) { delete q; }
