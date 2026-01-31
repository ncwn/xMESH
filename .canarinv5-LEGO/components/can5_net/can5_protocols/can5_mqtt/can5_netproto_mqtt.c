/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   09/02/22
*
* File:  can5_netstrat_udp.c
* Descr:
*******************************************************************************/

#include "can5_hazemon_types.h"
#include "can5_utils.h"
#include <esp_log.h>
#include "can5_netmng.h"
#include <stdlib.h>
#include <esp_timer.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <esp_event_base.h>
#include "can5_config.h"
#include "can5_netproto.h"
#include "can5_codec_hazemon.h"
#include "can5_netif_wrapper.h"
#include "can5_sensor_data.h"
#include "can5_events.h"
#include "can5_codec_mqtt.h"
#include "can5_mqtt_client.h"

static const char *TAG = "MQTT_STRAT";

#define CHECK_INITD()           if (__strat.status == MQTT_STRAT_UNINITD) return CAN5_ERR_INVALID_STATE
#define NET_MAX_SEND_ATTEMPTS   3 // attempts to send before reconnecting
#define RESET_SEND_ATTEMPTS()   { __strat.send_attempts = NET_MAX_SEND_ATTEMPTS; }

#define DEFAULT_NET_ACK_WAIT_TIMEOUT    5 //seconds

#define STORAGE_STACK_MAX_SEARCH_DEPTH  8 // this  defines the max records to search in the stack.

#define SEND_EVT(evt_type) {                    \
    if (__strat.send_cb) {                      \
        can5_netproto_evt_t evt = {             \
            .netproto_id = get_id(),            \
            .type = (evt_type),                 \
        };                                      \
        __strat.send_cb(&evt);                  \
    }                                           \
}

static uint8_t get_id();

static void set_id(uint8_t id);

static can5_err_t init(const can5_netif_t *netif);

static can5_err_t send_data(const uint8_t *data, size_t len, can5_netproto_send_cb_f send_cb);

static can5_err_t run();

static can5_err_t end();

static void forward_netif_connevt(const can5_net_connect_evt_t *evt);

static void forward_netif_rx(const void *data, size_t len);

const can5_netproto_t netproto_mqtt = {
    .type = CAN5_NETPROTO_MQTT,
    .get_id = get_id,
    .set_id = set_id,
    .init = init,
    .send = send_data,
    .run = run,
    .end = end,
    .forward_netif_connevt = forward_netif_connevt,
    .forward_netif_rx = forward_netif_rx
};

typedef enum mqtt_strat_status_e {
    MQTT_STRAT_UNINITD,
    MQTT_STRAT_INITD,
    MQTT_STRAT_READY,
    MQTT_STRAT_PROCESS,
    MQTT_STRAT_SEND,
    MQTT_STRAT_WAIT_ACK,
    MQTT_STRAT_COMPLETE,
    MQTT_STRAT_FAILED,
} mqtt_strat_status_t;

static struct mqtt_strat_s {
    volatile mqtt_strat_status_t status;
    uint8_t id;
    const can5_netif_t *netif;
    esp_event_handler_instance_t mqtt_evt_handler;
    can5_netproto_send_cb_f *send_cb;
    int send_attempts;
    can5_net_connect_evt_t netif_evt;
    struct {
        char *out_str;
        char out_timestamp[32];
        time_t last_sent;
        /* should be false if we have not popped the data yet */
    } tx;
    struct {
        can5_mqtt_data_rx_t data_rx;
        bool new_rx;
        bool wait_for_ack;
        int timeouts;
    } ack;
} __strat = {
    .status = MQTT_STRAT_UNINITD,
    .netif = NULL,
    .mqtt_evt_handler = NULL,
    .send_attempts = NET_MAX_SEND_ATTEMPTS,
    .netif_evt = {
        .type = CAN5_NET_CONNEVT_NONE,
    },
    .tx = {
        .out_str = NULL,
        .last_sent = 0,
    },
    .ack = {
        .data_rx = { 0 },
        .new_rx = false,
        .timeouts = 0,
        .wait_for_ack = false,
    }
};

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
static void __mqttclient_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                     void *event_data);

static void __state_set(mqtt_strat_status_t next);
static void __parse_rx_packet(const can5_mqtt_data_rx_t *data_rx);
static void __check_rx();
static can5_err_t __make_tx_packet(const char *data, size_t len);


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0

static const char* status_getstr(int32_t status);

#endif
/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

static uint8_t get_id()
{
    return __strat.id;
}

static void set_id(uint8_t id)
{
    __strat.id = id;
}

static can5_err_t init(const can5_netif_t *netif)
{

    if (__strat.status != MQTT_STRAT_UNINITD) {
        return CAN5_SUCCESS;
    }

    VERIFY_SUCCESS(esp_event_handler_instance_register(CAN5_EVT_MQTTCLIENT,
                                        ESP_EVENT_ANY_ID,
                                        __mqttclient_evt_handler,
                                        NULL,
                                        &__strat.mqtt_evt_handler));

    __strat.netif = netif;

    __state_set(MQTT_STRAT_INITD);

    return CAN5_SUCCESS;
}

static can5_err_t send_data(const uint8_t *data, size_t len, can5_netproto_send_cb_f send_cb)
{
    CHECK_INITD();
    ESP_LOGI(TAG, "Sending %s", (char *)data);

    __strat.send_cb = send_cb;

    if (__strat.status != MQTT_STRAT_READY) {
        return CAN5_NET_ERR_BUSY;
    }

    if (config_manager.read_bool(CFG_MQTT_WAIT_FOR_ACK, &__strat.ack.wait_for_ack) != CAN5_SUCCESS) {
        __strat.ack.wait_for_ack = false;
    }

    CLEAR_STRUCT(__strat.tx);
    VERIFY_SUCCESS(__make_tx_packet((const char *)data, len));

    __strat.ack.timeouts = 0;

    __state_set(MQTT_STRAT_PROCESS);

    return CAN5_SUCCESS;
}

static can5_err_t run()
{
    CHECK_INITD();
    __check_rx();
    int msg_id = 0;

    switch (__strat.status) {

        case MQTT_STRAT_UNINITD:
        case MQTT_STRAT_INITD:
            // nothing
            break;

        case MQTT_STRAT_READY:
            // just idle around
            break;

        case MQTT_STRAT_PROCESS:
            RESET_SEND_ATTEMPTS();
            __state_set(MQTT_STRAT_SEND);
            break;

        case MQTT_STRAT_SEND:
            // send packet

            if (__strat.send_attempts--) {
                const char *uris[] = {
                        "data"
                };

                msg_id = can5_mqtt_publish(uris, 1,
                                           __strat.tx.out_str, strlen(__strat.tx.out_str),
                                           0, false);
                if (msg_id != -1) {
                    __strat.tx.last_sent = can5_time(NULL);
                    if (__strat.ack.wait_for_ack) {
                        __state_set(MQTT_STRAT_WAIT_ACK);
                    }
                    else {
                        __state_set(MQTT_STRAT_COMPLETE);
                    }
                }
            }
            else {
                __state_set(MQTT_STRAT_FAILED);
                SEND_EVT(CAN5_NETPROTO_EVT_SEND_ATTEMPTS_OVER);
            }
            break;

        case MQTT_STRAT_WAIT_ACK:
            /* wait for parse packet to change state */
            if (__strat.tx.last_sent + DEFAULT_NET_ACK_WAIT_TIMEOUT < can5_time(NULL)) {
                /* we want to send infinite times till we get ack */
                //RESET_SEND_ATTEMPTS();
                __state_set(MQTT_STRAT_SEND);
                __strat.ack.timeouts++;

                if (__strat.ack.timeouts > CONFIG_CAN5_NET_HAZEMON_NOACK_THRESHOLD) {
                    // reset and try again
                    __state_set(MQTT_STRAT_FAILED);
                }
            }
            break;

        case MQTT_STRAT_COMPLETE:
            SEND_EVT(CAN5_NETPROTO_EVT_COMPLETE);
            free(__strat.tx.out_str);
            __strat.tx.out_str = NULL;
            __state_set(MQTT_STRAT_READY);
            break;

        case MQTT_STRAT_FAILED:
            SEND_EVT(CAN5_NETPROTO_EVT_FAILED);
            free(__strat.tx.out_str);
            __strat.tx.out_str = NULL;
            __state_set(MQTT_STRAT_READY);
            break;
    }

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0
    // ESP_LOGI(TAG, "%s", status_getstr(__strat.status));
#endif

    return CAN5_SUCCESS;
}

static can5_err_t end()
{
    switch (__strat.status) {

        case MQTT_STRAT_PROCESS:
        case MQTT_STRAT_SEND:
        case MQTT_STRAT_WAIT_ACK:
            __state_set(MQTT_STRAT_FAILED);
            break;
        default:
            break;
    }
    return CAN5_SUCCESS;
}

static void forward_netif_connevt(const can5_net_connect_evt_t *evt)
{
    __strat.netif_evt = *evt;

    if (__strat.netif_evt.type == CAN5_NET_CONNEVT_DISCONNECTED) {
    }
    ESP_LOGI(TAG, "Connection event: %s", connevt_getstr(evt->type));
}

static void forward_netif_rx(const void *data, size_t len)
{
    ESP_LOGE(TAG, "Not implemented %s", __func__);
}

/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/
static void __state_set(mqtt_strat_status_t next)
{
    __strat.status = next;
    __strat.netif_evt.type = CAN5_NET_CONNEVT_NONE;
}

static void __parse_rx_packet(const can5_mqtt_data_rx_t *data_rx) {

    if (!data_rx) {
        return;
    }

    memcpy(&__strat.ack.data_rx, data_rx, sizeof(can5_mqtt_data_rx_t));
    __strat.ack.new_rx = true;
}

static void __check_rx()
{
    bool ack_received;
    can5_mqtt_data_rx_t *data_rx;

    data_rx = &__strat.ack.data_rx;

    if (!__strat.ack.new_rx) {
        return;
    }

    ack_received = false;
    if (__strat.ack.wait_for_ack &&__strat.status == MQTT_STRAT_WAIT_ACK) {
        if (strcmp(__strat.tx.out_timestamp, data_rx->data) == 0) {
            config_manager.write_int(CFG_MQTT_STATS_LAST_ACK, time(NULL));
            ack_received = true;
        }
    }


    if(data_rx->data) {
        free(data_rx->data);
    }

    if (ack_received) {
        __state_set(MQTT_STRAT_COMPLETE);
    }
    else {
        __state_set(MQTT_STRAT_FAILED);
    }

    __strat.ack.new_rx = false;
}

static can5_err_t __make_tx_packet(const char *data, size_t len)
{
    can5_sensor_data_list_t list;
    can5_err_t ret;

    assert(!__strat.tx.out_str);

    TAILQ_INIT(&list);

    VERIFY_SUCCESS(can5_sensor_data_list_loads(data, &list));

    if ((ret = mqtt_make_tx(&list, &__strat.tx.out_str, __strat.tx.out_timestamp)) != CAN5_SUCCESS) {
        if (ret == CAN5_CODEC_ERR_NO_DATAPOINT || ret == CAN5_CODEC_ERR_NO_TIMESTAMP) {
            ret = CAN5_NET_ERR_PARSE_INCOMPLETE;
        }
        goto done;
    }

    if (!__strat.tx.out_str) {
        ret =  CAN5_NET_ERR_PARSE_INCOMPLETE;
        goto done;

    }

done:
    can5_sensor_data_list_free(&list);
    return ret;
}

static void __mqttclient_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                        void *event_data)
{

    if (event_base != CAN5_EVT_MQTTCLIENT) return;

    if (__strat.status == MQTT_STRAT_UNINITD) return;

    switch (event_id) {
        case CAN5_MQTTCLIENT_EVT_CONNECTED:
            __state_set(MQTT_STRAT_READY);
            break;
        case CAN5_MQTTCLIENT_EVT_DISCONNECTED:
            __state_set(MQTT_STRAT_INITD);
            break;
        case CAN5_MQTTCLIENT_EVT_DATA_ACK:
            __parse_rx_packet((can5_mqtt_data_rx_t *) event_data);
        default:
            break;
    }

}
//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0

static const can5_tag_tab_t  __udp_strat_stat_tags = {
    TAG_TAB_ITEM(MQTT_STRAT_UNINITD      ),
    TAG_TAB_ITEM(MQTT_STRAT_INITD        ),
    TAG_TAB_ITEM(MQTT_STRAT_READY        ),
    TAG_TAB_ITEM(MQTT_STRAT_PROCESS      ),
    TAG_TAB_ITEM(MQTT_STRAT_SEND         ),
    TAG_TAB_ITEM(MQTT_STRAT_WAIT_ACK     ),
    TAG_TAB_ITEM(MQTT_STRAT_COMPLETE     ),
    TAG_TAB_ITEM(MQTT_STRAT_FAILED       ),
};

__attribute__((unused)) static const char* status_getstr(int32_t status)
{
    return TAG_LOOKUP(status, __udp_strat_stat_tags);
}

#endif
