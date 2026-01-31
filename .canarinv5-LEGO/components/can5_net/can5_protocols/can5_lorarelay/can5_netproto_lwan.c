/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   09/02/22
*
* File:  can5_netstrategy_lwan.c
* Descr:
*******************************************************************************/
#include <stdlib.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "can5_config.h"
#include "can5_netproto.h"
#include "can5_utils.h"
#include "esp_log.h"
#include "can5_netmng.h"
#include "can5_codec_lwan.h"
#include "can5_netif_lwan.h"
#include "can5_sensor_data.h"
#include "can5_cmdr.h"

static const char *TAG = "LWAN_STRAT";

#define CHECK_INITD()           if (__strat.status == LWAN_STRAT_UNINITD) return CAN5_ERR_INVALID_STATE
#define NET_MAX_SEND_ATTEMPTS   3 // attempts to send before reconnecting
#define RESET_SEND_ATTEMPTS()   __strat.send_attempts = NET_MAX_SEND_ATTEMPTS

#define DEFAULT_NET_ACK_WAIT_TIMEOUT    8       // 8 seconds
#define DEFAULT_GPS_CYCLE               6       // send gps ever 5th cycle

#define STORAGE_STACK_MAX_SEARCH_DEPTH  32 // this  defines the max records to search in the stack.

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

const can5_netproto_t netproto_lwan = {
    .type = CAN5_NETPROTO_LORARELAY,
    .get_id = get_id,
    .set_id = set_id,
    .init = init,
    .send = send_data,
    .run = run,
    .end = end,
    .forward_netif_connevt = forward_netif_connevt,
    .forward_netif_rx = forward_netif_rx
};

typedef enum lwan_strat_status_e {
    LWAN_STRAT_UNINITD,
    LWAN_STRAT_INITD,
    LWAN_STRAT_READY,
    LWAN_STRAT_PROCESS,
    LWAN_STRAT_SEND,
    LWAN_STRAT_WAIT_SEND_CONFIRMATION,
    LWAN_STRAT_SEND_COMPLETE,
    LWAN_STRAT_WAIT_ACK,
    LWAN_STRAT_COMPLETE,
    LWAN_STRAT_FAILED,
} lwan_strat_status_t;

typedef struct buf_s {
    char buf[CAN5_STORAGE_MAX_LEN];
    size_t len;
} buf_t;

static struct lwan_strat_s {
    volatile lwan_strat_status_t status;
    uint8_t id;
    const can5_netif_t *netif;
    can5_netproto_send_cb_f *send_cb;
    bool hello_sent;
    int send_attempts;
    can5_net_connect_evt_t netif_evt;
    int8_t gps_cycle_idx;
    SemaphoreHandle_t send_mutex;
    bool running;
    struct {
        size_t curr_pkt;
        lwan_tx_pkt_list_t tx_pkts;
        time_t last_sent_ms;
    } tx;
} __strat = {
    .status = LWAN_STRAT_UNINITD,
    .netif = NULL,
    .hello_sent = false,
    .gps_cycle_idx = 0,
    .send_attempts = NET_MAX_SEND_ATTEMPTS,
    .netif_evt = {
        .type = CAN5_NET_CONNEVT_NONE,
    },
};

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
static void __state_set(lwan_strat_status_t next);

static can5_err_t __make_tx_packets(const char *data, size_t len);

static void __parse_rx_packet(const uint8_t *data, size_t len);
static void __mark_pkt_ack(uint8_t cycle_id, const uint8_t *ids, size_t len);
static bool __has_packet_seq_ack(const lwan_tx_pkt_t *tx_pkt);
static bool __has_cycle_ack();

static bool __send_gps();
static void __free_tx_pkt_list();

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
    if (__strat.status > LWAN_STRAT_UNINITD) {
        return CAN5_SUCCESS;
    }

    __strat.send_mutex = xSemaphoreCreateMutex();
    VERIFY_NOT_NULL(__strat.send_mutex);

    __strat.running = false;
    __strat.status = LWAN_STRAT_INITD;
    return CAN5_SUCCESS;
}

static can5_err_t send_data(const uint8_t *data, size_t len, can5_netproto_send_cb_f send_cb)
{
    CHECK_INITD();
    lwan_strat_status_t next_status;
    ESP_LOGI_V(TAG, "Sending %s", (char *)data);

    __strat.send_cb = send_cb;

    next_status = LWAN_STRAT_INITD;

    if (__strat.status == LWAN_STRAT_INITD) {
        next_status = LWAN_STRAT_PROCESS;
    }
    else if (__strat.status == LWAN_STRAT_READY) {
        next_status = LWAN_STRAT_PROCESS;
    }

    if (next_status != LWAN_STRAT_PROCESS) {
        return CAN5_ERR_INVALID_STATE;
    }

    CLEAR_STRUCT(__strat.tx);
    VERIFY_SUCCESS(__make_tx_packets((const char *)data, len));

    __state_set(next_status);
    __strat.running = true;

    return CAN5_SUCCESS;
}

#define TX_PKTS         __strat.tx.tx_pkts
#define CURR_TX_PKT_CTR __strat.tx.curr_pkt
#define CURR_TX_PKT     TX_PKTS.pkt[CURR_TX_PKT_CTR]

static can5_err_t run()
{
    CHECK_INITD();
    can5_err_t ret = CAN5_SUCCESS;
    size_t t_cur;


    switch (__strat.status) {

        case LWAN_STRAT_UNINITD:
        case LWAN_STRAT_INITD:
            // nothing
            break;

        case LWAN_STRAT_READY:
            // just idle around
            break;

        case LWAN_STRAT_PROCESS:
            RESET_SEND_ATTEMPTS();
            // make normal packet
            __state_set(LWAN_STRAT_SEND);
            break;

        case LWAN_STRAT_SEND:
            // if we are signalled to end
            if (!__strat.running) {
                __state_set(LWAN_STRAT_FAILED);
                break;
            }

            // send packet
            if (__strat.send_attempts--) {

                ESP_LOGE(TAG, "Sending %d Len %d", CURR_TX_PKT.cycle_id, CURR_TX_PKT.len);
                for (size_t i = 0; i < CURR_TX_PKT.num_seq; i ++) {
                    ESP_LOGE(TAG, "Seq %d", CURR_TX_PKT.seq_id[i]);
                }

                ret = __strat.netif->ops.send(CURR_TX_PKT.pkt, CURR_TX_PKT.len);

                if (ret == CAN5_SUCCESS) {
                    // update packet to send
                    __state_set(LWAN_STRAT_WAIT_SEND_CONFIRMATION);

                }
                else if (ret == CAN5_NET_ERR_BUSY) {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    __state_set(LWAN_STRAT_PROCESS);
                }
                else {
                    CAN5_ERR_CHECK_NO_ABORT(ret);
                    __state_set(LWAN_STRAT_FAILED);
                }

            }

            else {
                // TODO: all send events failed, reset lorawan
                ESP_LOGE(TAG, "Send Attempts exhausted");
                __state_set(LWAN_STRAT_FAILED);
                SEND_EVT(CAN5_NETPROTO_EVT_SEND_ATTEMPTS_OVER);
            }
            break;

        case LWAN_STRAT_WAIT_SEND_CONFIRMATION:
            switch (__strat.netif_evt.type) {

                case CAN5_NET_CONNEVT_SENDING:
                    __state_set(LWAN_STRAT_SEND_COMPLETE);

                    break;
                case CAN5_NET_CONNEVT_SEND_FAILED:
                    __state_set(LWAN_STRAT_SEND);

                    break;
                default:
                    break;
            }
            break;

        case LWAN_STRAT_SEND_COMPLETE:
            __strat.tx.last_sent_ms = can5_time_ms(NULL);
            t_cur = (CURR_TX_PKT_CTR + 1) % TX_PKTS.count;
            while (t_cur != CURR_TX_PKT_CTR) {
                if (__has_packet_seq_ack(&TX_PKTS.pkt[t_cur])) {
                    t_cur = (t_cur + 1) % TX_PKTS.count;
                }
                else {
                    break;
                }
            }

            CURR_TX_PKT_CTR = t_cur;
            __state_set(LWAN_STRAT_WAIT_ACK);
            break;

        case LWAN_STRAT_WAIT_ACK:
            // because some sends also saves firmware context which takes extra time
            vTaskDelay(pdMS_TO_TICKS(1000));
            switch (__strat.netif_evt.type) {

                case CAN5_NET_CONNEVT_SEND_COMPLETE:
                    if (__has_cycle_ack()) {
                        RESET_SEND_ATTEMPTS();
                        __state_set(LWAN_STRAT_COMPLETE);
                    }
                    else {
                        __state_set(LWAN_STRAT_PROCESS);
                    }
                    break;
                default:
                    break;
            }
            break;

        case LWAN_STRAT_COMPLETE:
            __free_tx_pkt_list();
            SEND_EVT(CAN5_NETPROTO_EVT_COMPLETE);
            __state_set(LWAN_STRAT_READY);
            __strat.running = false;
            break;

        case LWAN_STRAT_FAILED:
            __free_tx_pkt_list();
            SEND_EVT(CAN5_NETPROTO_EVT_FAILED);
            __state_set(LWAN_STRAT_INITD);
            __strat.running = false;
            break;
    }

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0
    //ESP_LOGI(TAG, "%s", status_getstr(__strat.status));
#endif

    return CAN5_SUCCESS;
}

static can5_err_t end()
{
    __strat.running = false;
    return CAN5_SUCCESS;
}

static void forward_netif_connevt(const can5_net_connect_evt_t *evt)
{
    __strat.netif_evt = *evt;
    ESP_LOGI(TAG, "Connection event: %s", connevt_getstr(evt->type));
}

static void __mark_pkt_ack(uint8_t cycle_id, const uint8_t *seq, size_t len)
{
    for (int i = 0; i < TX_PKTS.count && i < LWAN_MAX_PACKETS_PER_CYCLE; i++) {
        lwan_tx_pkt_t *pkt = &TX_PKTS.pkt[i];
        if (pkt->cycle_id != cycle_id) {
            continue;
        }
        for(int j = 0; j < pkt->num_seq && j < LWAN_MAX_DATA_PER_CYCLE; j++) {
            for (int k =0; k < len; k++) {
                if (seq[k] == pkt->seq_id[j]) {
                    ESP_LOGE(TAG, "ack for cycle %d pkt %d seq %d", cycle_id, i, seq[k]);
                    pkt->ack_received[j] = true;
                    break;
                }
            }
        }
    }
}

static bool __has_packet_seq_ack(const lwan_tx_pkt_t *tx_pkt)
{
    for(int i = 0; i < tx_pkt->num_seq && i < LWAN_MAX_DATA_PER_CYCLE; i++) {
        printf("here seq %i %s\n", i, boolean_get_str(tx_pkt->ack_received[i]));
        if (!tx_pkt->ack_received[i])  {
            return false;
        }
    }
    return true;
}

static bool __has_cycle_ack()
{
    for (int i = 0 ; i < TX_PKTS.count && i < 32; i++) {
        printf("here pkt %in\n" ,i);
        if(!__has_packet_seq_ack(&TX_PKTS.pkt[i])) {
            return false;
        }
    }
    return true;
}

static void forward_netif_rx(const void *data, size_t len)
{
    __parse_rx_packet(data, len);
    ESP_LOG_BUFFER_HEXDUMP_V(TAG, data, len, ESP_LOG_INFO);
}

/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/
static void __state_set(lwan_strat_status_t next)
{
    __strat.status = next;
    __strat.netif_evt.type = CAN5_NET_CONNEVT_NONE;
}

static void __dispatch_rx_cmds(lwan_rx_pkt_t *response)
{
    can5_cmd_params_t params;
    switch (response->type) {

        case LWAN_RX_CMD_REBOOT:
            params.restart_after = 1;
            can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &params, NULL, NULL);
            break;
        case LWAN_RX_CMD_INTERVAL:
            config_manager.write_int(CFG_DATA_CYCLE_SEC, response->args);
            params.restart_after = 1;
            can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &params, NULL, NULL);
            break;
        case LWAN_RX_CMD_RESET_FRAME_CTR:
            params.cmdr_event.timeout_ms = 1000;
            params.cmdr_event.event = CAN5_CMDR_EVT_LWAN_RESET_FRAME_COUNT;
            can5_commander.add_cmd(CAN5_CMD_POST_EVENT, &params, NULL, NULL);
            break;
        case LWAN_RX_CMD_REJOIN:
            ESP_LOGE(TAG, "LWAN_RX_CMD_REJOIN not implemented");
            break;
        default:
            break;
    }
}

static void __parse_rx_packet(const uint8_t *data, size_t len)
{
    lwan_rx_pkt_t response;
    can5_err_t ret;

    if (!len || !data) {
        return;
    }

    CLEAR_STRUCT(response);
    ret = lwan_parse_rx_packet(data, len, &response);

    if (response.type != LWAN_RX_CMD_DATA) {
        // dispatch to some other command function
        __dispatch_rx_cmds(&response);
        return;
    }

    ESP_LOGE_V(TAG, "Received cycle %d", response.data.cycle_id);
    for(size_t i = 0; i < LWAN_MAX_DATA_PER_CYCLE; i++) {
        if (response.data.seq_id[i]) {
            ESP_LOGI_V(TAG, "seq %d", i);
        }
    }

    if (ret == CAN5_SUCCESS) {
        if (__strat.status == LWAN_STRAT_WAIT_ACK) {
            __mark_pkt_ack(response.data.cycle_id, response.data.seq_id, response.data.num_seq);
            ESP_LOGI(TAG, "RX cycle id %d :Successfully parsed!", response.data.cycle_id);
        }
        else {
            ESP_LOGI(TAG, "RX cycle id %d :Parsed but discarded!", response.data.cycle_id);
        }
    }
    else CAN5_ERR_CHECK_NO_ABORT(ret);
}

static can5_err_t __make_tx_packets(const char *data, size_t len)
{
    can5_sensor_data_list_t list;
    can5_err_t ret;
    int64_t pkt_id, data_rate;
    int64_t user_max_bytes;
    can5_sensor_data_t *sensor_data;
    bool has_gps;

    bool send_gps = __send_gps();

    lwan_make_tx_packets_args_t lwan_args = {
        .skip_gps = !send_gps,
        .num_cycle_data = 0,
    };

    VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_PKT_ID, &pkt_id));
    VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_DATA_RATE, &data_rate));
    VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_USER_DATALEN, &user_max_bytes));
    VERIFY_SUCCESS(config_manager.read_bool(CFG_LWAN_ADD_NUM_CYCLE_DATA, &lwan_args.add_num_cycle_data));

    lwan_args.cycle_ctr = pkt_id;
    lwan_args.datarate = (int)data_rate;
    lwan_args.user_max_bytes = user_max_bytes;

    TAILQ_INIT(&list);

    VERIFY_SUCCESS(can5_sensor_data_list_loads(data, &list));

    if (!lwan_args.skip_gps) {
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

    }



    ESP_LOGI(TAG, "Sending data:");
    TAILQ_FOREACH(sensor_data, &list, te) {
        // count the expected number of cycle data, excluding NUM_CYCLE_DATA packets
        if (lwan_args.skip_gps) {
            if (!can5_sensor_data_is_gps_type(sensor_data->type)) {
                lwan_args.num_cycle_data++;
            }
        }
        else {
            lwan_args.num_cycle_data++;
        }
        ESP_LOGI(TAG, "%s: %s", can5_sensor_data_type_getstr(sensor_data->type), sensor_data->val);
    }

    // remove timestamp data point as it in not a sensor data point, but packet metadata
    lwan_args.num_cycle_data--;

    ESP_LOGI(TAG, "num packets:%d send_gps: %s",lwan_args.num_cycle_data, boolean_get_str(send_gps));

    if ((ret = lwan_make_tx_packets(&list, &__strat.tx.tx_pkts, &lwan_args)) != CAN5_SUCCESS) {
        if (ret == CAN5_CODEC_ERR_NO_DATAPOINT || ret == CAN5_CODEC_ERR_NO_TIMESTAMP) {
            ret = CAN5_NET_ERR_PARSE_INCOMPLETE;
        }
        goto done;
    }

    if (!__strat.tx.tx_pkts.count) {
        ret = CAN5_NET_ERR_PARSE_INCOMPLETE;
        goto done;
    }

    VERIFY_SUCCESS(config_manager.write_int(CFG_LWAN_PKT_ID, (pkt_id + 1) % 4));

done:
    can5_sensor_data_list_free(&list);
    return ret;
}

static void __free_tx_pkt_list()
{
    for(size_t i = 0; i < TX_PKTS.count; i++ ) {
        can5_lmsg_free_packet(TX_PKTS.pkt[i].pkt);
    }
    CLEAR_STRUCT(TX_PKTS);
    CURR_TX_PKT_CTR = 0;
}

static bool __send_gps()
{
    bool result = false;
    int64_t gps_per_cycle = DEFAULT_GPS_CYCLE;
    if (config_manager.read_int(CFG_LORARELAY_GPS_CYCLE, &gps_per_cycle) == CAN5_SUCCESS) {
        if (gps_per_cycle == 0) {
            gps_per_cycle = DEFAULT_GPS_CYCLE;
        }
    }

    if (!__strat.gps_cycle_idx) {
        result = true;
    }
    __strat.gps_cycle_idx = (__strat.gps_cycle_idx  + 1) % gps_per_cycle;

    return result;
}

//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0

static const can5_tag_tab_t  __lwan_strat_stat_tags = {
    TAG_TAB_ITEM(LWAN_STRAT_UNINITD      ),
    TAG_TAB_ITEM(LWAN_STRAT_INITD        ),
    TAG_TAB_ITEM(LWAN_STRAT_PROCESS      ),
    TAG_TAB_ITEM(LWAN_STRAT_SEND         ),
    TAG_TAB_ITEM(LWAN_STRAT_WAIT_SEND_CONFIRMATION      ),
    TAG_TAB_ITEM(LWAN_STRAT_SEND_COMPLETE),
    TAG_TAB_ITEM(LWAN_STRAT_WAIT_ACK     ),
    TAG_TAB_ITEM(LWAN_STRAT_COMPLETE     ),
    TAG_TAB_ITEM(LWAN_STRAT_FAILED       ),

};

__attribute__((unused)) static const char* status_getstr(int32_t status)
{
    return TAG_LOOKUP(status, __lwan_strat_stat_tags);
}

#endif
