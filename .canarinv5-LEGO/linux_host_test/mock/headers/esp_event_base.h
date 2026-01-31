/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   02/02/22
*
* File:  esp_event_base.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_ESP_EVENT_BASE_H
#define TEST_APP_ESP_EVENT_BASE_H

typedef const char*  esp_event_base_t;

#define ESP_EVENT_DECLARE_BASE(id) extern esp_event_base_t id
#define ESP_EVENT_DEFINE_BASE(id) esp_event_base_t id = #id

typedef void*        esp_event_loop_handle_t; /**< a number that identifies an event with respect to a base */
typedef void         (*esp_event_handler_t)(void* event_handler_arg,
                                        esp_event_base_t event_base,
                                        int32_t event_id,
                                        void* event_data); /**< function called when an event is posted to the queue */
typedef void*        esp_event_handler_instance_t; /**< context identifying an instance of a registered event handler */

// Defines for registering/unregistering event handlers
#define ESP_EVENT_ANY_BASE     NULL             /**< register handler for any event base */
#define ESP_EVENT_ANY_ID       -1               /**< register handler for any event id */

#endif //TEST_APP_ESP_EVENT_BASE_H
