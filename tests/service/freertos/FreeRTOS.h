#pragma once
#include <stdint.h>
#include <stddef.h>
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
