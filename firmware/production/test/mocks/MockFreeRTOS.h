/**
 * @file MockFreeRTOS.h
 * @brief Mock FreeRTOS primitives for native unit testing
 * 
 * This header provides mock implementations of FreeRTOS types and functions
 * used by xmesh-core modules, enabling unit testing on the host PC without
 * requiring the actual ESP32 FreeRTOS implementation.
 * 
 * Usage: Include this header BEFORE including xmesh headers in test files.
 */

#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#ifdef NATIVE_BUILD

#include <cstdint>
#include <cstddef>

// ============================================================
// FreeRTOS Type Definitions
// ============================================================

typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

// FreeRTOS constants
#define pdTRUE          1
#define pdFALSE         0
#define pdPASS          pdTRUE
#define pdFAIL          pdFALSE
#define portMAX_DELAY   0xFFFFFFFFUL

// ============================================================
// Mock Time Functions
// ============================================================

namespace mock {
    static uint32_t millis_value = 0;
    static uint32_t random_seed = 12345;
    static bool zero_random = false;  // If true, random() returns 0 for predictable testing
    
    inline void reset() {
        millis_value = 0;
        random_seed = 12345;
        zero_random = false;
    }
    
    inline void set_millis(uint32_t value) {
        millis_value = value;
    }
    
    inline void advance_millis(uint32_t delta) {
        millis_value += delta;
    }
    
    inline void set_zero_random(bool enabled) {
        zero_random = enabled;
    }
}

// Arduino-compatible millis() mock
inline uint32_t millis() {
    return mock::millis_value;
}

inline long random(long min, long max) {
    if (mock::zero_random) return min;
    mock::random_seed = mock::random_seed * 1103515245 + 12345;
    return min + (mock::random_seed % (max - min));
}

inline long random(long max) {
    if (mock::zero_random) return 0;
    return random(0, max);
}

// ============================================================
// Mock Semaphore Functions
// ============================================================

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    return reinterpret_cast<SemaphoreHandle_t>(0x1);
}

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return reinterpret_cast<SemaphoreHandle_t>(0x2);
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime) {
    (void)xSemaphore;
    (void)xBlockTime;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
    return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
}

// ============================================================
// Mock Queue Functions
// ============================================================

inline QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    (void)uxQueueLength;
    (void)uxItemSize;
    return reinterpret_cast<QueueHandle_t>(0x3);
}

inline BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
    (void)xQueue;
    (void)pvItemToQueue;
    (void)xTicksToWait;
    return pdTRUE;
}

inline BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    (void)xQueue;
    (void)pvBuffer;
    (void)xTicksToWait;
    return pdFALSE;  // No items in mock queue
}

inline void vQueueDelete(QueueHandle_t xQueue) {
    (void)xQueue;
}

// ============================================================
// Mock Task Functions
// ============================================================

inline void vTaskDelay(TickType_t xTicksToDelay) {
    mock::millis_value += xTicksToDelay;
}

inline BaseType_t xTaskCreate(
    void (*pxTaskCode)(void*),
    const char* pcName,
    uint32_t usStackDepth,
    void* pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t* pxCreatedTask
) {
    (void)pxTaskCode;
    (void)pcName;
    (void)usStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    if (pxCreatedTask) *pxCreatedTask = reinterpret_cast<TaskHandle_t>(0x4);
    return pdPASS;
}

// ============================================================
// ESP-IDF Logging Stubs
// ============================================================

#define ESP_LOGE(tag, fmt, ...) do { (void)tag; } while(0)
#define ESP_LOGW(tag, fmt, ...) do { (void)tag; } while(0)
#define ESP_LOGI(tag, fmt, ...) do { (void)tag; } while(0)
#define ESP_LOGD(tag, fmt, ...) do { (void)tag; } while(0)
#define ESP_LOGV(tag, fmt, ...) do { (void)tag; } while(0)

// ============================================================
// ESP-IDF System Stubs
// ============================================================

inline size_t esp_get_free_heap_size() {
    return 200000;  // 200KB free heap (healthy)
}

inline size_t esp_get_minimum_free_heap_size() {
    return 150000;  // 150KB minimum
}

#endif // NATIVE_BUILD

#endif // MOCK_FREERTOS_H
