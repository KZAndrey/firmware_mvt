#pragma once
#include "FreeRTOS.h"
using TimerHandle_t=void *;
inline TimerHandle_t xTimerCreate(const char *,unsigned,int,void *,void (*)(TimerHandle_t)){return nullptr;}
inline int xTimerStart(TimerHandle_t,unsigned){return pdPASS;}
inline int xTimerStop(TimerHandle_t,unsigned){return pdPASS;}
inline int xTimerDelete(TimerHandle_t,unsigned){return pdPASS;}
