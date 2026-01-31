/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   09/02/22
*
* File:  can5_codec_lwan.c
* Descr:
*******************************************************************************/
#include "can5_config.h"
#include <stdlib.h>
#include "can5_utils.h"
#include <esp_log.h>
#include "can5_loramsg.h"
#include "can5_codec_lwan.h"
#include "can5_sensor_data.h"


#if 0
const static char *TAG = "CODEC_LWAN";
#define TRACE_FUNC ESP_LOGI_V(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

static const size_t data_rate_table[7] = {
    // max bytes            // data-rate
    0,                      // 0
    0,                      // 1
    11,                     // 2
    49,                     // 3
    120,                    // 4
    230,                    // 5
};


#define S_TO_LR_MAP(stype, htype)    {.sensor_data_type = (stype), .lr_data_type = (htype) }

static const struct {
    can5_sensor_data_type_t sensor_data_type;
    enum can5_lmsg_sensor_type lr_data_type;
} __sensor_to_lr_type[] = {


    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_NUM_CYCLE_DATA, LMSG_NUM_CYCLE_DATA),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT, LMSG_GLAT),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG, LMSG_GLNG),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT, LMSG_GALT),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT, LMSG_GLAT),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG, LMSG_GLNG),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT, LMSG_GALT),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_CF1, LMSG_PM1),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_CF1, LMSG_PM2_5),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_CF1, LMSG_PM10),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_MH_Z16_CO2, LMSG_MHZ16CO2),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_ZE03_NO2, LMSG_NO2),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_ZE07_CO, LMSG_CO),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_BME280_TEMP, LMSG_TEMP),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_BME280_PRES, LMSG_PRES),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_BME280_HUMI, LMSG_HUMI),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_RAIN, LMSG_RAIN),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD, LMSG_WIND_SPD),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR, LMSG_WIND_DIR),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT, LMSG_VOLT),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT, LMSG_VOLT_PERCENTAGE),

    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_SCD41_CO2, LMSG_MHZ16CO2),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_SCD41_TEMP, LMSG_TEMP),
    S_TO_LR_MAP(CAN5_SENSOR_DATA_TYPE_SCD41_HUMI, LMSG_HUMI),
};
/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t
__make_lwan_packet(const can5_sensor_data_list_t *list, lwan_tx_pkt_list_t *out_pkt_list,
                   const lwan_make_tx_packets_args_t *args, size_t max_bytes,
                   can5_sensor_data_t **start_from, uint8_t *curr_seq_id, int64_t timestamp,
                   const can5_sensor_data_t *num_cycle_data);

static can5_err_t __make_num_cycle_data(const lwan_make_tx_packets_args_t *args, can5_sensor_data_t *data);

/* ---------------------------------------------------------------------
 * Function definitions
 -----------------------------------------------------------------------*/

can5_err_t lwan_make_tx_packets(const can5_sensor_data_list_t *list, lwan_tx_pkt_list_t *out_pkt_list,
                                const lwan_make_tx_packets_args_t *args)
{
    size_t max_bytes;
    uint8_t seq_id;
    int64_t timestamp;
    bool success;
    can5_sensor_data_t *timestamp_data, *starting_node, *num_cycle_data;

    if ( 0 > args->datarate || args->datarate > 5) {
        return CAN5_ERR_INVALID_PARAM;
    }

    if (args->user_max_bytes) {
        max_bytes = args->user_max_bytes;
    }
    else {
        max_bytes = data_rate_table[args->datarate];
    }

    timestamp_data = can5_sensor_data_get_timestamp(list);

    if (!timestamp_data) {
        return CAN5_CODEC_ERR_NO_TIMESTAMP;
    }

    // negative value
    if (timestamp_data->val[0] == '-') {
        return CAN5_NET_ERR_PARSE_INCOMPLETE;
    }

    timestamp = can5_sensor_data_get_num(timestamp_data, &success, false);

    if (!success) {
        return CAN5_CODEC_ERR_NO_TIMESTAMP;
    }


    if (args->num_cycle_data) {
        VERIFY_ALLOC(num_cycle_data, sizeof(can5_sensor_data_t));
        if (__make_num_cycle_data(args, num_cycle_data) != CAN5_SUCCESS) {
            return CAN5_ERR_OUT_OF_HEAP_MEMORY;
        }
    }
    else {
        num_cycle_data = NULL;
    }

    starting_node = TAILQ_FIRST(list);


    out_pkt_list->count = 0;
    seq_id = 0;

    // this should consume all the sensor_data and make multiple packets
    while (starting_node &&
        __make_lwan_packet(list, out_pkt_list, args, max_bytes, &starting_node, &seq_id, timestamp, num_cycle_data) == CAN5_SUCCESS) {
        out_pkt_list->count++;
    }

    if (args->num_cycle_data) {
        can5_sensor_data_free(num_cycle_data);
    }


    return CAN5_SUCCESS;
}

enum can5_lmsg_sensor_type can5_map_get_lr_type(can5_sensor_data_type_t type)
{
    for (size_t i = 0; i < sizeof(__sensor_to_lr_type) / sizeof(__sensor_to_lr_type[0]); i++) {
        if (type == __sensor_to_lr_type[i].sensor_data_type) {
            return __sensor_to_lr_type[i].lr_data_type;
        }
    }

    return LMSG_NR_ERROR;
}

static can5_err_t __make_num_cycle_data(const lwan_make_tx_packets_args_t *args, can5_sensor_data_t *data)
{
    // length of 4 bytes to store upto 32
    VERIFY_ALLOC(data->val, 4);
    snprintf(data->val, 4, "%d", args->num_cycle_data);

    data->type = CAN5_SENSOR_DATA_TYPE_NUM_CYCLE_DATA;
    data->datatype = CAN5_SENSOR_DATA_DATATYPE_NUM;
    data->port = CAN5_PORT_NULL;

    return CAN5_SUCCESS;
}

static can5_err_t __make_lwan_packet_data(const can5_sensor_data_t *cur, struct can5_lmsg_data **s_data,
    size_t *s_data_idx, size_t *curr_bytes, const lwan_make_tx_packets_args_t *args, const uint8_t *curr_seq_id)
{
    enum can5_lmsg_sensor_type lmsg_type;
    enum can5_lmsg_data_dtype lmsg_dtype;

    lmsg_type = can5_map_get_lr_type(cur->type);

    if (lmsg_type == LMSG_NR_ERROR) {
        return CAN5_CODEC_ERR_INVALID_DATATYPE;
    }

    lmsg_dtype = can5_lmsg_get_datatype(lmsg_type);

    bool success;
    double result;
    char *result_s;

    switch (lmsg_dtype) {

        case LMSG_DT_INT:
        case LMSG_DT_FLOAT:
            result = can5_sensor_data_get_dec(cur, &success,false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            s_data[*s_data_idx] = can5_lmsg_make_sensor_data(lmsg_type,
                                                            args->cycle_ctr, *curr_seq_id,
                                                            result);
            break;


        case LMSG_DT_STR:
            result_s = can5_sensor_data_get_str(cur, false);

            s_data[*s_data_idx] = can5_lmsg_make_sensor_data_string(lmsg_type,
                                                                   args->cycle_ctr, *curr_seq_id,
                                                                   result_s, strlen(result_s));
            free(result_s);

            break;
            // TODO: implement string
        case LMSG_DT_INVALID:
        case LMSG_DT_MAX:
        default:
            return CAN5_CODEC_ERR_INVALID_DATATYPE;
    }

    (*curr_bytes) += can5_lmsg_get_data_size(s_data[*s_data_idx]);
    (*s_data_idx)++;

    return CAN5_SUCCESS;
}

static can5_err_t
__make_lwan_packet(const can5_sensor_data_list_t *list, lwan_tx_pkt_list_t *out_pkt_list,
                   const lwan_make_tx_packets_args_t *args, size_t max_bytes,
                   can5_sensor_data_t **start_from, uint8_t *curr_seq_id, int64_t timestamp,
                   const can5_sensor_data_t *num_cycle_data)
{
    size_t curr_bytes;

    can5_sensor_data_t *cur = *start_from;

    struct can5_lmsg_data *s_data[32];
    size_t s_data_idx;

    lwan_tx_pkt_t *tx_pkt = &out_pkt_list->pkt[out_pkt_list->count];

    curr_bytes = 0;
    s_data_idx = 0;
    tx_pkt->num_seq = 0;

    if (args->add_num_cycle_data) {
        // best try effort
        __make_lwan_packet_data(num_cycle_data, s_data, &s_data_idx, &curr_bytes, args, curr_seq_id);
        tx_pkt->seq_id[tx_pkt->num_seq++] = *curr_seq_id;
        (*curr_seq_id)++;
    }

    TAILQ_FOREACH_FROM(cur, list, te) {
        // skip timestamp
        if (cur->type == CAN5_SENSOR_DATA_TYPE_TIMESTAMP) {
            continue;
        }

        // skip gps
        if (args->skip_gps && can5_sensor_data_is_gps_type(cur->type)) {
            continue;
        }

        if (__make_lwan_packet_data(cur, s_data, &s_data_idx, &curr_bytes, args, curr_seq_id) != CAN5_SUCCESS) {
            continue;
        }


        if (s_data_idx == 1) {
            // only one packet
            if (curr_bytes + sizeof(struct can5_lmsg_single_packet) > max_bytes) {
                s_data_idx--;
                can5_lmsg_free_sensor_data(s_data[s_data_idx]);
                break;

            }
        }
        else if (s_data_idx > 1) {
            if (curr_bytes + sizeof(struct can5_lmsg_multi_packet) > max_bytes) {
                s_data_idx--;
                can5_lmsg_free_sensor_data(s_data[s_data_idx]);
                break;
            }
        }
        // set the current sequence number
        tx_pkt->seq_id[tx_pkt->num_seq++] = *curr_seq_id;

        // update the sequence number
        (*curr_seq_id)++;
    }

    *start_from = cur;

    if (s_data_idx >= 1) {
        tx_pkt->pkt = can5_lmsg_make_packet(timestamp, s_data, s_data_idx, &tx_pkt->len);
        tx_pkt->cycle_id = args->cycle_ctr;


        for(size_t i = 0; i < s_data_idx; i ++) {
            can5_lmsg_free_sensor_data(s_data[i]);
        }
        can5_lmsg_hton_packet(tx_pkt->pkt);
        return CAN5_SUCCESS;
    }
    else {
        tx_pkt->num_seq = 0;
        tx_pkt->len = 0;
        return CAN5_CODEC_ERR_NO_DATAPOINT;
    }
}

can5_err_t lwan_parse_rx_packet(const uint8_t *data, size_t buf_len, lwan_rx_pkt_t *res)
{
    struct can5_lmsg_packet *pkt;
    pkt = (struct can5_lmsg_packet *) data;

    if (can5_lmsg_ntoh_packet_ack(pkt, (int) buf_len) == 0) {
        return CAN5_NET_ERR_PARSE_INCOMPLETE;
    }

    switch (pkt->ack.type) {

        case LMSG_ACK_TYPE_DATA:
            res->type = LWAN_RX_CMD_DATA;
            res->data.cycle_id = pkt->ack.cycle_id;
            res->data.num_seq = 0;
            for (int i = 0; i < 32; i ++) {
                if (pkt->ack.data & (1 << i)) {
                    res->data.seq_id[res->data.num_seq++] =  i;
                }
            }
            break;
        case LMSG_ACK_TYPE_RESTART:
            res->type = LWAN_RX_CMD_REBOOT;
            break;
        case LMSG_ACK_TYPE_INTERVAL:
            res->type = LWAN_RX_CMD_INTERVAL;
            res->args = pkt->ack.data;
            break;
        case LMSG_ACK_TYPE_RESET_FRAME_COUNTER:
            res->type = LWAN_RX_CMD_RESET_FRAME_CTR;
            break;
        case LMSG_ACK_TYPE_REJOIN:
            res->type = LWAN_RX_CMD_REJOIN;
            break;
        default:
            return CAN5_NET_ERR_PARSE_INCOMPLETE;
    }

    return CAN5_SUCCESS;
}

static const can5_tag_tab_t __loramsg_tags = {
    TAG_TAB_ITEM(LMSG_GLAT            ),
    TAG_TAB_ITEM(LMSG_GLNG            ),
    TAG_TAB_ITEM(LMSG_GALT            ),
    TAG_TAB_ITEM(LMSG_TEMP            ),
    TAG_TAB_ITEM(LMSG_HUMI            ),
    TAG_TAB_ITEM(LMSG_PRES            ),
    TAG_TAB_ITEM(LMSG_PM2_5           ),
    TAG_TAB_ITEM(LMSG_PM10            ),
    TAG_TAB_ITEM(LMSG_PM1             ),
    TAG_TAB_ITEM(LMSG_CO              ),
    TAG_TAB_ITEM(LMSG_MQ7CO           ),
    TAG_TAB_ITEM(LMSG_MHZ16CO2        ),
    TAG_TAB_ITEM(LMSG_NO2             ),
    TAG_TAB_ITEM(LMSG_VOLT            ),
    TAG_TAB_ITEM(LMSG_RAIN            ),
    TAG_TAB_ITEM(LMSG_WIND_SPD        ),
    TAG_TAB_ITEM(LMSG_WIND_DIR        ),
    TAG_TAB_ITEM(LMSG_NUM_CYCLE_DATA  ),
    TAG_TAB_ITEM(LMSG_VOLT_PERCENTAGE ),

    TAG_TAB_ITEM(LMSG_WZSHCHO         ),
    TAG_TAB_ITEM(LMSG_FORM            ),
    TAG_TAB_ITEM(LMSG_STATUS          ),

    TAG_TAB_ITEM(LMSG_ACK             ),
    TAG_TAB_ITEM(LMSG_NR_ERROR        ),
    TAG_TAB_ITEM(LMSG_NR_INTERVAL     ),
    TAG_TAB_ITEM(LMSG_MAX             ),
};

const char *loramsg_sensor_type_str(int type)
{
    return TAG_LOOKUP(type, __loramsg_tags);
}
