#pragma once
#include <cstddef>
#include <cstdint>
using StackType_t=uint8_t;
struct StaticTask_t { unsigned placeholder; };
constexpr int pdTRUE=1;
constexpr unsigned portMAX_DELAY=~0u;
constexpr int pdPASS=1,pdFALSE=0;
#define pdMS_TO_TICKS(ms) (ms)
using TaskHandle_t=void *;
TaskHandle_t xTaskCreateStatic(void (*)(void *),const char *,unsigned,void *,unsigned,StackType_t *,StaticTask_t *);
unsigned ulTaskNotifyTake(int,unsigned);
int xTaskNotifyGive(TaskHandle_t);
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
void vTaskDelete(void *);
void vTaskDelay(unsigned);
