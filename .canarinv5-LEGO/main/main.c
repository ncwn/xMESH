#include "freertos/FreeRTOS.h"
#include "can5.h"
#include "esp_log.h"
#include "can5_error.h"
#include "can5.h"
#include "can5_utils.h"

#define TAG "main"

void app_main(void)
{
    ESP_LOGI(TAG, "Canarin awake");
    ESP_LOGE(TAG, "Wakeup cause: %d", esp_sleep_get_wakeup_cause());
    can5_err_t ret;
    bool ota_upgrade;

#if defined(CONFIG_CAN5_HAL_BUILD_TESTS) || defined(CONFIG_CAN5_RTC_BUILD_TESTS)        \
    || defined(CONFIG_CAN5_SENSOR_BUILD_TESTS) || defined(CONFIG_CAN5_NET_BUILD_TESTS)  \
    || defined(CONFIG_CAN5_CONFIG_BUILD_TESTS)
    can5_tests();
#endif

    if ((ret = can5_modules_init(&ota_upgrade)) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK(ret);
        ESP_LOGE(TAG, "Initialization Failure! Restart...");
        can5_restart();
    }

    can5_run(ota_upgrade);
}
