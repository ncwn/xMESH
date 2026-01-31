/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_rtc_test.c
* Descr:
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "can5_error.h"
#include "esp_log.h"
#include "can5_rtc_test.h"
#include "can5_rtc.h"

static const char *TAG = "RTC_TEST";

#define CHECK_RESULT(r) do { \
int ret;  \
ret = r;  \
if (ret == CAN5_SUCCESS) ESP_LOGI(TAG, "OK!");  else CAN5_ERR_CHECK_NO_ABORT(ret);   \
} while (0)


#define CHECK_RESULT_FAIL(r) do { \
int ret;  \
ret = r;  \
if (ret != CAN5_SUCCESS) ESP_LOGI(TAG, "OK Failed %d!", ret);  \
} while (0)

void alarm_callback()
{
    ESP_LOGI(TAG, "Alarm callback");
}

can5_err_t rtc_test_start() {
    static char buffer[255];
    static struct timeval tv;
    static struct tm tm;
    rtc.get_time(&tm);

    tm.tm_sec += 10;
    mktime(&tm);

    CHECK_RESULT(rtc.set_alarm(&tm));
    // GPIO light sleep wakeup
    CHECK_RESULT(rtc.sleep());

    int i = 0;
    CHECK_RESULT(rtc.rtc_to_sys());
    while (i++ < 15) {
        CHECK_RESULT(rtc.get_time(&tm));
        asctime_r(&tm, buffer);
        ESP_LOGI(TAG, "RTC Time: %s", buffer);
        gettimeofday(&tv, NULL);
        asctime_r(gmtime(&tv.tv_sec), buffer);
        ESP_LOGI(TAG, "Sys Time: %s", buffer);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    i = 0;
    // drift systime by 100 seconds in future
    tv.tv_sec += 100;
    settimeofday(&tv, NULL);
    while (i++ < 5) {
        CHECK_RESULT(rtc.get_time(&tm));
        asctime_r(&tm, buffer);
        ESP_LOGI(TAG, "RTC Time: %s", buffer);
        gettimeofday(&tv, NULL);
        asctime_r(gmtime(&tv.tv_sec), buffer);
        ESP_LOGI(TAG, "Sys Time: %s", buffer);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // sync rtc to sys time
    CHECK_RESULT(rtc.sys_to_rtc());
    CHECK_RESULT(rtc.get_time(&tm));
    tm.tm_sec += 10;
    mktime(&tm);

    CHECK_RESULT(rtc.set_alarm(&tm));
    CHECK_RESULT(rtc.sleep());

    i = 0;
    while (i++ < 15) {
        CHECK_RESULT(rtc.get_time(&tm));
        asctime_r(&tm, buffer);
        ESP_LOGI(TAG, "RTC Time: %s", buffer);
        gettimeofday(&tv, NULL);
        asctime_r(gmtime(&tv.tv_sec), buffer);
        ESP_LOGI(TAG, "Sys Time: %s", buffer);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return CAN5_SUCCESS;
}