/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   04/02/22
*
* File:  esp_event.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_ESP_EVENT_H
#define TEST_APP_ESP_EVENT_H
#include "esp_event_base.h"

#define esp_event_handler_instance_t void *

typedef int TickType_t;


esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int, esp_event_handler_t handler, void * args,
                                              esp_event_handler_instance_t instance);


esp_err_t esp_event_post(esp_event_base_t event_base,
                         int32_t event_id,
                         void *event_data,
                         size_t event_data_size,
                         TickType_t ticks_to_wait);

#endif //TEST_APP_ESP_EVENT_H
