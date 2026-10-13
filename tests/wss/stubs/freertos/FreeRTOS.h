#pragma once
#include <deque>
#include <cstdint>
typedef int TickType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(n) (n)
struct TestQueue { size_t max; std::deque<char *> values; };
typedef TestQueue *QueueHandle_t;
inline QueueHandle_t xQueueCreate(int max, size_t) { return new TestQueue{size_t(max), {}}; }
inline int xQueueSend(QueueHandle_t q, char **v, int) {
    if (q->values.size() >= q->max) return 0;
    q->values.push_back(*v); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, char **v, int) {
    if (q->values.empty()) return 0;
    *v = q->values.front(); q->values.pop_front(); return pdTRUE;
}
inline void vQueueDelete(QueueHandle_t q) { delete q; }
