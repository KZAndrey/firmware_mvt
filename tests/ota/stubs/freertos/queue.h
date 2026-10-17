#pragma once
#include "FreeRTOS.h"
using QueueHandle_t=void *;
QueueHandle_t xQueueCreate(unsigned,size_t);
int xQueueReset(QueueHandle_t);
int xQueueOverwrite(QueueHandle_t,const void *);
int xQueueSend(QueueHandle_t,const void *,unsigned);
int xQueueReceive(QueueHandle_t,void *,unsigned);
