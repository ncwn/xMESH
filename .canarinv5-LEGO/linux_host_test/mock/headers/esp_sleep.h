/**************************************************
 * Author: rmukhia
 * Creation Date: 17/11/22
 * Description: 
 **************************************************/

#ifndef TEST_APP_ESP_SLEEP_H
#define TEST_APP_ESP_SLEEP_H

static int esp_register_shutdown_handler(void (*param)(void)) {
    return 0;
}

static int esp_sleep_enable_timer_wakeup(int i) {
    return 0;
}

#define esp_deep_sleep_start()
#endif //TEST_APP_ESP_SLEEP_H
