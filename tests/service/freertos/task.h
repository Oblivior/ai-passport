#pragma once
#include "FreeRTOS.h"
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *task);
void vTaskDelay(uint32_t ticks);
