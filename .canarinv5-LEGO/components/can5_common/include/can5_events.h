/* esp_event (event loop library) basic example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef __CAN5_EVENTS_H__
#define __CAN5_EVENTS_H__

#include "esp_event.h"

// // Declare an event base
// ESP_EVENT_DECLARE_BASE(TIMER_EVENTS);        // declaration of the timer events family

// enum {                                       // declaration of the specific events under the timer event family
//     TIMER_EVENT_STARTED,                     // raised when the timer is first started
//     TIMER_EVENT_EXPIRY,                      // raised when a period of the timer has elapsed
//     TIMER_EVENT_STOPPED                      // raised when the timer has been stopped
// };

// ESP_EVENT_DECLARE_BASE(TASK_EVENTS);         // declaration of the task events family

// enum {
//     TASK_ITERATION_EVENT,                    // raised during an iteration of the loop within the task
// };

typedef enum can5_hal_evt_e {
    CAN5_HAL_EVT_NONE        = 0x00,
    CAN5_HAL_EVT_INITIALIZED       ,
    CAN5_HAL_EVT_NETPORT_RECVD     ,
    CAN5_HAL_EVT_UPORT_RECVD       ,
    CAN5_HAL_EVT_WAKE_INT_RISE     ,
    CAN5_HAL_EVT_WAKE_INT_FALL     ,
    CAN5_HAL_EVT_USRBTN_INT_RISE   ,
    CAN5_HAL_EVT_USRBTN_INT_FALL   ,
    CAN5_HAL_EVT_RTC_INT_RISE      ,
    CAN5_HAL_EVT_RTC_INT_FALL      ,
    CAN5_HAL_EVT_USRBTN_PRESS      ,
    CAN5_HAL_EVT_USRBTN_LONG_PRESS ,
    CAN5_HAL_EVT_WIFI_STA_CONNECTED,
    CAN5_HAL_EVT_WIFI_STA_DISCONNECTED,
    CAN5_HAL_EVT_WIFI_AP_START,
    CAN5_HAL_EVT_WIFI_AP_CLIENT_JOINED,
    CAN5_HAL_EVT_CELL_OPERATOR,
    CAN5_HAL_EVT_CELL_CONNECTED    ,
    CAN5_HAL_EVT_CELL_DISCONNECTED ,
} can5_hal_evt_t;

ESP_EVENT_DECLARE_BASE(CAN5_EVT_HAL);


typedef enum can5_evt_scheduler_e {
    CAN5_SCHEDULER_EVT_PRE_READ = 0,
    CAN5_SCHEDULER_EVT_READ,
    CAN5_SCHEDULER_EVT_NET_START,
    CAN5_SCHEDULER_EVT_TRIGGER_SLEEP,
} can5_evt_scheduler_t;

ESP_EVENT_DECLARE_BASE(CAN5_EVT_SCHEDULER);


typedef enum can5_sensormng_evt_e {
    CAN5_SENSORMNG_EVT_NONE = 0,
    CAN5_SENSORMNG_EVT_SLEEP_UNTIL,
    CAN5_SENSORMNG_EVT_CYCLE_SAVED,
    CAN5_SENSORMNG_EVT_INITIALIZED,
    CAN5_SENSORMNG_EVT_COMPLETE,
    CAN5_SENSORMNG_EVT_BUSY,
} can5_sensormng_evt_t;
ESP_EVENT_DECLARE_BASE(CAN5_EVT_SENSORMNG);


typedef enum can5_net_evt_e {
    CAN5_NET_EVT_NONE   = 0x00,
    CAN5_NET_EVT_SLEEP_UNTIL,
    CAN5_NET_EVT_INITIALIZED,
    CAN5_NET_EVT_RUNNING,
    CAN5_NET_EVT_BUSY,                      /* Net busy */
    CAN5_NET_EVT_COMPLETED,                 /* Communication completed */
    CAN5_NET_EVT_RECVD,
    CAN5_NET_EVT_LORAWAN_GOT_DEVEUI,
} can5_net_evt_t;
ESP_EVENT_DECLARE_BASE(CAN5_EVT_NET);


typedef enum can5_storagemng_evt_e {
    CAN5_STORAGEMNG_EVT_NONE = 0,
    CAN5_STORAGEMNG_EVT_INITIALIZED,
    CAN5_STORAGEMNG_EVT_PUSHED,
    CAN5_STORAGEMNG_EVT_POPPED,
} can5_storagemng_evt_t;
ESP_EVENT_DECLARE_BASE(CAN5_EVT_STORAGEMNG);


typedef enum can5_cmdr_evt_e {
    CAN5_CMDR_EVT_NONE = 0,
    CAN5_CMDR_EVT_LWAN_PAUSE,
    CAN5_CMDR_EVT_LWAN_RESUME,
    CAN5_CMDR_EVT_LWAN_RESET_FRAME_COUNT,
} can5_cmdr_evt_t;

ESP_EVENT_DECLARE_BASE(CAN5_EVT_CMDR);


typedef enum can5_mqttclient_evt_e {
    CAN5_MQTTCLIENT_EVT_NONE = 0,
    CAN5_MQTTCLIENT_EVT_CONNECTED,
    CAN5_MQTTCLIENT_EVT_DISCONNECTED,
    CAN5_MQTTCLIENT_EVT_DATA_ACK,
} can5_mqttclient_evt_t;

ESP_EVENT_DECLARE_BASE(CAN5_EVT_MQTTCLIENT);


#endif // #ifndef _CAN5_EVENTS_H_
