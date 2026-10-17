#pragma once
#include <cstddef>
constexpr int pdPASS=1,pdFALSE=0;
#define pdMS_TO_TICKS(ms) (ms)
using TaskHandle_t=void *;
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
void vTaskDelete(void *);
void vTaskDelay(unsigned);
