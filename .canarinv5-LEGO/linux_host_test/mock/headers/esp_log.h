/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   30/01/22
*
* File:  esp_log.h
* Descr:
*******************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifndef TEST_APP_ESP_LOG_H
#define TEST_APP_ESP_LOG_H

#define ESP_LOGI(TAG, fmt, ...) { _log(TAG, fmt, ##__VA_ARGS__); }

#define ESP_LOGW(TAG, fmt, ...) { _log(TAG, fmt, ##__VA_ARGS__); }

#define ESP_LOGD(TAG, fmt, ...) { _log(TAG, fmt, ##__VA_ARGS__); }

#define ESP_LOGE(TAG, fmt, ...) {       \
    printf("\033[0;32m");               \
    _log(TAG, fmt, ##__VA_ARGS__);      \
    printf("\033[0m");                  \
}

typedef enum esp_log_level_e {
    ESP_LOG_INFO,
    ESP_LOG_ERROR,
} esp_log_level_t;

#define ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, level)   { \
    switch(level)                                         \
    {                                                     \
        case ESP_LOG_INFO:                                \
            _log_hex(TAG, data, len);                     \
            break;                                        \
        case ESP_LOG_ERROR:                               \
            printf("\033[0;32m");                         \
            _log_hex(TAG, data, len);                     \
            printf("\033[0m");                            \
            break;                                        \
    }                                                     \
}

static inline void _log(const char *tag, const char *fmt, ...)
{
	char buf[8196];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	printf("[%ld][%s]: %s\n", time(NULL), tag, buf);
}

static inline void _log_hex(const char *tag, const char *data, const size_t len)
{
    printf("[%ld][%s]: ", time(NULL), tag);
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", (unsigned char)(data[i]));
    }
    printf("\n");
}


typedef int (*vprintf_like_t)(const char*, va_list);

#define esp_log_set_vprintf(vprintf_like_t)     NULL
#endif //TEST_APP_ESP_LOG_H
