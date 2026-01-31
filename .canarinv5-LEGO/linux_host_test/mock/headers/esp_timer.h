/**************************************************
 * Author: rmukhia
 * Creation Date: 11/7/22
 * Description: 
 **************************************************/

#ifndef TEST_APP_ESP_TIMER_H
#define TEST_APP_ESP_TIMER_H

static time_t esp_timer_get_time() {
    return time(NULL) * 10000000;
}

#endif //TEST_APP_ESP_TIMER_H
