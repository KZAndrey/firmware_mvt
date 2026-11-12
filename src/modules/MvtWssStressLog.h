#pragma once
// The normal logger allocates a copy of its format string. Heap-pressure
// instrumentation must not use it: that observer can itself abort on OOM.
#if defined(ESP_PLATFORM)
#include "esp_log.h"
#define MVT_STRESS_LOG(...) ESP_LOGW("MVT_TEST", __VA_ARGS__)
#else
#define MVT_STRESS_LOG(...) LOG_WARN(__VA_ARGS__)
#endif
