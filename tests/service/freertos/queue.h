#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned count, size_t size);
int xQueueSend(QueueHandle_t queue, const void *item, uint32_t wait);
int xQueueReceive(QueueHandle_t queue, void *item, uint32_t wait);
void vQueueDelete(QueueHandle_t queue);
