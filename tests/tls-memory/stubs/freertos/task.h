#pragma once
#include "FreeRTOS.h"
extern thread_local TaskHandle_t testCurrentTask;
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return testCurrentTask; }
inline bool xPortInIsrContext() { return false; }
