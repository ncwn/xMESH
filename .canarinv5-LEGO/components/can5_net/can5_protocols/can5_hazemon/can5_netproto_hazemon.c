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
#include "can5_config.h"
#include "can5_netproto.h"
#include "can5_codec_hazemon.h"
#include "can5_netif_wrapper.h"
#include "can5_sensor_data.h"

static const char *TAG = "HMON_STRAT";

#define CHECK_INITD()           if (__strat.status == HMON_STRAT_UNINITD) return CAN5_ERR_INVALID_STATE
#define NET_MAX_SEND_ATTEMPTS   3 // attempts to send before reconnecting
#define RESET_SEND_ATTEMPTS()   { __strat.send_attempts = NET_MAX_SEND_ATTEMPTS; __strat.ack_waiting = false; }

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

const can5_netproto_t netproto_hazemon = {
    .type = CAN5_NETPROTO_HAZEMON,
    .get_id = get_id,
    .set_id = set_id,
    .init = init,
    .send = send_data,
    .run = run,
    .end = end,
    .forward_netif_connevt = forward_netif_connevt,
    .forward_netif_rx = forward_netif_rx
};

typedef enum hmon_strat_status_e {
    HMON_STRAT_UNINITD,
    HMON_STRAT_INITD,
    HMON_STRAT_CONNECT,
    HMON_STRAT_READY,
    HMON_STRAT_PROCESS,
    HMON_STRAT_SEND_HELLO_PACKET,
    HMON_STRAT_SEND,
    HMON_STRAT_WAIT_ACK,
    HMON_STRAT_COMPLETE,
    HMON_STRAT_FAILED,
} hmon_strat_status_t;

typedef struct buf_s {
    uint8_t buf[CONFIG_CAN5_UDP_MTU];
    size_t len;
} buf_t;

static struct hmon_strat_s {
    volatile hmon_strat_status_t status;
    uint8_t id;
    const can5_netif_t *netif;
    can5_netproto_send_cb_f *send_cb;
    int send_attempts;
    can5_net_connect_evt_t netif_evt;
    struct {
        int sock;
        struct sockaddr_in dest_addr;
    } end_point;
    bool ack_waiting;
    int ack_timeouts;
    bool hello_pkt_sent;
    struct {
        /* ready to send network packet */
        buf_t pkt;

        /* should free this after hello packet is sent */
        struct {
            uint8_t buf[32];    // during development, I constantly encountered 20
            size_t len;
        } hello_pkt;

        time_t last_sent;
        /* should be false if we have not popped the data yet */
    } tx;
} __strat = {
    .status = HMON_STRAT_UNINITD,
    .netif = NULL,
    .send_attempts = NET_MAX_SEND_ATTEMPTS,
    .netif_evt = {
        .type = CAN5_NET_CONNEVT_NONE,
    },
    .end_point = {
        .sock = -1,
    },
    .ack_waiting = false,
    .ack_timeouts = 0,
    .hello_pkt_sent = false,
    .tx = {
        .pkt = {
            .buf = { 0 },
            .len = 0,
        },
        .last_sent = 0,
    }
};

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
static void __state_set(hmon_strat_status_t next);
static void __parse_rx_packet(const uint8_t *data, size_t len);
static can5_err_t __make_tx_packet(const char *data, size_t len);
static can5_err_t __make_hello_data();
static can5_err_t __open_udp_socket();
static void __close_socket();
static can5_err_t __send(const void *data, size_t len);

static void __recv_non_blocking();

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
    __strat.netif = netif;
    __strat.status = HMON_STRAT_INITD;
    return CAN5_SUCCESS;
}

static can5_err_t send_data(const uint8_t *data, size_t len, can5_netproto_send_cb_f send_cb)
{
    CHECK_INITD();
    hmon_strat_status_t next_status;
    ESP_LOGI_V(TAG, "Sending %s", (char *)data);

    __strat.send_cb = send_cb;

    next_status = HMON_STRAT_INITD;

    if (__strat.status == HMON_STRAT_INITD) {
        next_status = HMON_STRAT_CONNECT;
    }
    else if (__strat.status == HMON_STRAT_READY) {
        next_status = HMON_STRAT_PROCESS;
    }

    if (!(next_status == HMON_STRAT_CONNECT || next_status == HMON_STRAT_PROCESS)) {
        return CAN5_ERR_INVALID_STATE;
    }

    CLEAR_STRUCT(__strat.tx);
    VERIFY_SUCCESS(__make_tx_packet((const char *)data, len));

    __strat.ack_waiting = false;
    __strat.ack_timeouts = 0;


    __state_set(next_status);

    return CAN5_SUCCESS;
}

static can5_err_t run()
{
    CHECK_INITD();

    __recv_non_blocking();

    switch (__strat.status) {

        case HMON_STRAT_UNINITD:
        case HMON_STRAT_INITD:
            // nothing
            break;

        case HMON_STRAT_CONNECT:
            if (__open_udp_socket() == CAN5_SUCCESS) {
                __state_set(HMON_STRAT_PROCESS);
            }
            break;

        case HMON_STRAT_READY:
            // just idle around
            break;

        case HMON_STRAT_PROCESS:
            RESET_SEND_ATTEMPTS();
            if (!__strat.hello_pkt_sent) {
                __make_hello_data();
                __state_set(HMON_STRAT_SEND_HELLO_PACKET);
            }
            else {
                __state_set(HMON_STRAT_SEND);
            }
            break;

        case HMON_STRAT_SEND_HELLO_PACKET:
            if (__strat.send_attempts--) {
                if (__send(__strat.tx.hello_pkt.buf, __strat.tx.hello_pkt.len) == CAN5_SUCCESS) {
                    __strat.tx.last_sent = can5_time(NULL);
                    __strat.ack_waiting = true;              // we are waiting for ack now
                    __state_set(HMON_STRAT_WAIT_ACK);
                }
            }
            else {
                // if we cannot send for send_attempts
                __close_socket();
                __state_set(HMON_STRAT_FAILED);
                SEND_EVT(CAN5_NETPROTO_EVT_SEND_ATTEMPTS_OVER);
            }
            break;

        case HMON_STRAT_SEND:
            // send packet
            if (__strat.send_attempts--) {
                if (__send(__strat.tx.pkt.buf, __strat.tx.pkt.len) == CAN5_SUCCESS) {
                    __strat.tx.last_sent = can5_time(NULL);
                    __strat.ack_waiting = true;              // we are waiting for ack now
                    __state_set(HMON_STRAT_WAIT_ACK);
                }
            }
            else {
                // if we cannot send for send_attempts
                __close_socket();
                __state_set(HMON_STRAT_FAILED);
                SEND_EVT(CAN5_NETPROTO_EVT_SEND_ATTEMPTS_OVER);
            }
            break;

        case HMON_STRAT_WAIT_ACK:
            /* wait for parse packet to change state */
            if (__strat.tx.last_sent + DEFAULT_NET_ACK_WAIT_TIMEOUT < can5_time(NULL)) {
                /* we want to send infinite times till we get ack */
                // RESET_SEND_ATTEMPTS();
                if (__strat.hello_pkt_sent) {
                    __state_set(HMON_STRAT_SEND);
                }
                else {
                    __state_set(HMON_STRAT_SEND_HELLO_PACKET);
                }
                __strat.ack_timeouts++;

                if (__strat.ack_timeouts > CONFIG_CAN5_NET_HAZEMON_NOACK_THRESHOLD) {
                    // reset and try again
                    __close_socket();
                    __state_set(HMON_STRAT_FAILED);
                }

            }
            break;

        case HMON_STRAT_COMPLETE:
            SEND_EVT(CAN5_NETPROTO_EVT_COMPLETE);
            __state_set(HMON_STRAT_READY);
            break;

        case HMON_STRAT_FAILED:
            SEND_EVT(CAN5_NETPROTO_EVT_FAILED);
            __state_set(HMON_STRAT_INITD);


    }

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0
    // ESP_LOGI(TAG, "%s", status_getstr(__strat.status));
#endif

    return CAN5_SUCCESS;
}

static can5_err_t end()
{
    return CAN5_SUCCESS;
}

static void forward_netif_connevt(const can5_net_connect_evt_t *evt)
{
    __strat.netif_evt = *evt;

    if (__strat.netif_evt.type == CAN5_NET_CONNEVT_DISCONNECTED) {
        __close_socket();
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
static void __state_set(hmon_strat_status_t next)
{
    __strat.status = next;
    __strat.netif_evt.type = CAN5_NET_CONNEVT_NONE;
}

static void __parse_rx_packet(const uint8_t *data, size_t len)
{
    hazemon_rx_cmd_list_t rx_list;
    hazemon_rx_cmd_t *cmd;
    can5_err_t ret;

    if (!len) {
        return;
    }

    // reset the ack timeout count
    __strat.ack_timeouts = 0;

    TAILQ_INIT(&rx_list);

    ret = hazemon_parse_rx_packet(data, len, &rx_list);

    CAN5_ERR_CHECK_NO_ABORT(ret);

    if (ret == CAN5_SUCCESS) {
        ESP_LOGI_V(TAG, "wait for ack %d timeout count %d", __strat.ack_waiting, __strat.ack_timeouts);
        if (__strat.ack_waiting) {
            /* receiving an ack when hello sent is false means, its ack for hello packet? */
            if (__strat.status == HMON_STRAT_WAIT_ACK) {

                // process if hello packet has been sent.
                if (!__strat.hello_pkt_sent) {
                    __strat.hello_pkt_sent = true;
                    __state_set(HMON_STRAT_SEND);
                }
                else {
                    __state_set(HMON_STRAT_COMPLETE);
                }

            }
            ESP_LOGI(TAG, "RX Successfully parsed!");
        }
        else {
            ESP_LOGE(TAG, "Parsed but discarded!");
        }
    }
    else {
        CAN5_ERR_CHECK_NO_ABORT(ret);
        __state_set(HMON_STRAT_FAILED);
    }

    ESP_LOGI(TAG, "Received Upstream commands:");
    size_t count = 0;
    TAILQ_FOREACH(cmd, &rx_list, te) {
        ESP_LOGI(TAG, "Cmd[%d]: %s: %s", count, can5_hazemon_get_type(cmd->type)->token, cmd->val);


        // todo move to commander
        if (cmd->type == HAZEMON_INTERVAL) {
            bool sync_interval;
            if (config_manager.read_bool(CFG_HAZEMON_SYNC_INTERVAL, &sync_interval) != CAN5_SUCCESS) {
                sync_interval = false;
            }

            if (sync_interval) {
                char *null_ptr;
                int64_t sec;
                sec = strtol(cmd->val, &null_ptr, 10);

                if (sec != 0 && cmd->val != null_ptr) {
                    config_manager.write_int(CFG_DATA_CYCLE_SEC, atoi(cmd->val) * 60);
                }
            }
        }

        count++;
    }




    hazemon_rx_cmd_list_free(&rx_list, false);
}

static can5_err_t __make_tx_packet(const char *data, size_t len)
{
    can5_sensor_data_list_t list;
    can5_err_t ret;
    can5_sensor_data_t *sensor_data;
    bool has_gps;

    TAILQ_INIT(&list);

    VERIFY_SUCCESS(can5_sensor_data_list_loads(data, &list));

    has_gps = false;
    TAILQ_FOREACH(sensor_data, &list, te) {
        has_gps = can5_sensor_data_is_gps_type(sensor_data->type);
        if (has_gps) {
            break;
        }
    }

    if (!has_gps) {
        double lat, lng, alt;
        if ((ret = config_manager.read_double(CFG_LAST_G_LAT, &lat)) != CAN5_SUCCESS) {
            goto done;
        }

        if ((ret = config_manager.read_double(CFG_LAST_G_LNG, &lng)) != CAN5_SUCCESS) {
            goto done;
        }

        if ((ret = config_manager.read_double(CFG_LAST_G_ALT, &alt)) != CAN5_SUCCESS) {
            goto done;
        }

        sensor_data = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT, CAN5_PORT_NULL,
                                               CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, lat);
        TAILQ_INSERT_TAIL(&list, sensor_data, te);

        sensor_data = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG, CAN5_PORT_NULL,
                                               CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, lng);
        TAILQ_INSERT_TAIL(&list, sensor_data, te);

        sensor_data = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT, CAN5_PORT_NULL,
                                               CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, alt);
        TAILQ_INSERT_TAIL(&list, sensor_data, te);
    }

    ESP_LOGI_V(TAG, "Sending data:");
    TAILQ_FOREACH(sensor_data, &list, te) {
        ESP_LOGI_V(TAG, "%s: %s", can5_sensor_data_type_getstr(sensor_data->type), sensor_data->val);
    }



    if ((ret = hazemon_make_tx_packet(&list, (uint8_t *)__strat.tx.pkt.buf, &__strat.tx.pkt.len)) != CAN5_SUCCESS) {
        if (ret == CAN5_CODEC_ERR_NO_DATAPOINT || ret == CAN5_CODEC_ERR_NO_TIMESTAMP) {
            ret = CAN5_NET_ERR_PARSE_INCOMPLETE;
        }
        goto done;
    }

    if (!__strat.tx.pkt.len) {
        ret =  CAN5_NET_ERR_PARSE_INCOMPLETE;
        goto done;

    }

done:
    can5_sensor_data_list_free(&list);
    return ret;
}

static can5_err_t __make_hello_data()
{
    can5_sensor_data_list_t list;
    can5_err_t ret;
    can5_sensor_data_t *sensor_data;


    sensor_data = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_TIMESTAMP,
                                           CAN5_PORT_NULL,
                                           CAN5_SENSOR_DATA_DATATYPE_NUM,
                                           0, 0, 0);


    TAILQ_INIT(&list);
    TAILQ_INSERT_HEAD(&list, sensor_data, te);


    ESP_LOGI_V(TAG, "Sending data:");
    TAILQ_FOREACH(sensor_data, &list, te) {
        ESP_LOGI_V(TAG, "%s: %s", can5_sensor_data_type_getstr(sensor_data->type), sensor_data->val);
    }
    CLEAR_ARRAY(__strat.tx.hello_pkt.buf);

    ret = hazemon_make_tx_packet(&list, (uint8_t *)__strat.tx.hello_pkt.buf, &__strat.tx.hello_pkt.len);

    CAN5_ERR_CHECK_NO_ABORT(ret);

    ESP_LOG_BUFFER_HEXDUMP_V(TAG, __strat.tx.hello_pkt.buf, __strat.tx.hello_pkt.len, ESP_LOG_INFO);
    can5_sensor_data_list_free(&list);
    return ret;

}

static can5_err_t __open_udp_socket()
{
    char *hazemon_ip;
    int64_t hazemon_port;

    if (!__strat.netif->ops.is_connected()) {
        return CAN5_NET_ERR_DISCONNECTEDIF;
    }

#if 0
    if (__strat.end_point.sock >= 0) {
        return CAN5_SUCCESS;
    }
#endif

    hazemon_ip = NULL;

    VERIFY_SUCCESS(config_manager.read_int(CFG_HAZEMON_PORT, &hazemon_port));
    VERIFY_SUCCESS(config_manager.read(CFG_HAZEMON_IP, (uint8_t **)&hazemon_ip, NULL));

    ESP_LOGI_V(TAG, "Host IP %s, port %d", config.hazemon_ip_port->ip, config.hazemon_ip_port->port);

    __strat.end_point.dest_addr.sin_addr.s_addr = inet_addr(hazemon_ip);
    __strat.end_point.dest_addr.sin_port = htons(hazemon_port);

    free(hazemon_ip);

    __strat.end_point.dest_addr.sin_family = AF_INET;

    if (__strat.end_point.sock >= 0) {
        closesocket(__strat.end_point.sock);
    }

    __strat.end_point.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (__strat.end_point.sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return CAN5_NET_ERR_CONN_TIMEOUT;
    }

    return CAN5_SUCCESS;
}

static void __close_socket()
{
    if (__strat.end_point.sock >= 0) {
        closesocket(__strat.end_point.sock);
    }
    __strat.end_point.sock = -1;
}

static can5_err_t __send(const void *data, size_t len)
{
    ESP_LOGI_V(TAG, "[%d]Sending %s", len, (char *)data);

    if (sendto(__strat.end_point.sock, data, len, 0,
               (struct sockaddr *) &__strat.end_point.dest_addr,
                   sizeof(__strat.end_point.dest_addr)) < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %s", strerror(errno));
        return CAN5_ERROR;
    }

    return CAN5_SUCCESS;
}

static void __recv_non_blocking()
{
    static char buf[NETIF_WPPR_MAX_RECV_SIZE];
    static ssize_t buf_len;

    if (!__strat.netif->ops.is_connected()) {
        return;
    }
    buf_len = recv(__strat.end_point.sock, buf, NETIF_WPPR_MAX_RECV_SIZE, MSG_DONTWAIT);

    if (buf_len > 0) {
        __parse_rx_packet((uint8_t  *)buf, buf_len);
    }
    else {
        taskYIELD();
    }
}

//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0

static const can5_tag_tab_t  __udp_strat_stat_tags = {
    TAG_TAB_ITEM(HMON_STRAT_UNINITD),
    TAG_TAB_ITEM(HMON_STRAT_INITD),
    TAG_TAB_ITEM(HMON_STRAT_CONNECT),
    TAG_TAB_ITEM(HMON_STRAT_READY),
    TAG_TAB_ITEM(HMON_STRAT_PROCESS),
    TAG_TAB_ITEM(HMON_STRAT_SEND_HELLO_PACKET),
    TAG_TAB_ITEM(HMON_STRAT_SEND),
    TAG_TAB_ITEM(HMON_STRAT_WAIT_ACK),
    TAG_TAB_ITEM(HMON_STRAT_COMPLETE),
    TAG_TAB_ITEM(HMON_STRAT_FAILED),
};

__attribute__((unused)) static const char* status_getstr(int32_t status)
{
    return TAG_LOOKUP(status, __udp_strat_stat_tags);
}

#endif
