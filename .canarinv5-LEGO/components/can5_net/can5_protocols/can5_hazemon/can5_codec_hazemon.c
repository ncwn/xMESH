/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   09/02/22
*
* File:  can5_codec_udp.c
* Descr:
*******************************************************************************/
/*
 * TLV logic is copied from existing hazemon code.
 * Its-
 * VL - Variable Length Encoding
 * datapoint - VL(type) + VL(length(data)) + data
 * packet - VL(device_id) + timestamp:uint64_t + length(packet):uint16_t + datapoint_1
 *                  + datapoint_2 + ... + datapoint_n + crc:uint8_t
 *
 *  Everything in little endian in hazemon system.
 */
#include <sys/time.h>
#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <endian.h>
#include "esp_log.h"
#include "can5_config.h"
#include "can5_rtc.h"
#include "can5_error.h"
#include "can5_hazemon_types.h"
#include "can5_codec_hazemon.h"
#include "can5_utils.h"
#include "can5_sensor_data.h"

#define SERVER_TIME_THRESHOLD_SEC   60
#define CRC_LEN                     1

#ifndef CAN5_LINUX_HOST_TEST
#define HAZEMON_SERVER_SYC_TIME
#endif

static const char *TAG = "NETCODEC";

#if 0
#define TRACE_FUNC ESP_LOGI_V(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define HAZEMON_NONE   HAZEMON_TYPE_LAST


/* ---------------------------------------------------------------------
 * Sensor Data to Hazemon mapping
 -----------------------------------------------------------------------*/
#define S_TO_H_MAP(stype, htype)    {.sensor_data_type = (stype), .hazemon_type = (htype) }

static const struct {
    can5_sensor_data_type_t sensor_data_type;
    hazemon_type_t hazemon_type;
} __sensor_to_hazemon_type[] = {

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_TIMESTAMP, HAZEMON_TIMESTAMP),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT, HAZEMON_GPS_LAT),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG, HAZEMON_GPS_LNG),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT, HAZEMON_GPS_ALT),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_NORTH, HAZEMON_AX),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_EAST, HAZEMON_AY),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_DOWN, HAZEMON_AZ),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT, HAZEMON_GPS_LAT),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG, HAZEMON_GPS_LNG),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT, HAZEMON_GPS_ALT),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_CF1, HAZEMON_PM1_0),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_CF1, HAZEMON_PM2_5),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_CF1, HAZEMON_PM10),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_ATM, HAZEMON_NONE),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_ATM, HAZEMON_NONE),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_ATM, HAZEMON_NONE),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_MH_Z16_CO2, HAZEMON_MHZ16_CO2),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_ZE03_NO2, HAZEMON_NO2),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_ZE07_CO, HAZEMON_CO),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_BME280_TEMP, HAZEMON_TEMPERATURE),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_BME280_PRES, HAZEMON_PRESSURE),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_BME280_HUMI, HAZEMON_HUMIDITY),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_RAIN, HAZEMON_RAIN),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD, HAZEMON_WIND_SPD),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR, HAZEMON_WIND_DIR),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT, HAZEMON_BATT_V),

    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_SCD41_CO2, HAZEMON_MHZ16_CO2),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_SCD41_TEMP, HAZEMON_TEMPERATURE),
    S_TO_H_MAP(CAN5_SENSOR_DATA_TYPE_SCD41_HUMI, HAZEMON_HUMIDITY),
};

typedef enum hazemon_datatype_e {
    DT_UNSIGNED_CHAR = 0,           // 1 bytes
    DT_FLOAT,                       // 4 bytes
    DT_SHORT,                       // 2 bytes
    DT_UNSIGNED_SHORT,              // 2 bytes
    DT_STRING,                      // string
    DT_UNSIGNED_LONG,               // 4 bytes
    DT_UNSIGNED_LONG_LONG,          // 8 bytes
    DT_INVALID,
} hazemon_datatype_t;


/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t __make_hazemon_packet(const can5_sensor_data_list_t *list,
                                        const can5_sensor_data_t *time_sensor_data,
                                        uint8_t *out_pkt, size_t *out_len);

static can5_err_t __parse_hazemon_packet(const uint8_t *data, size_t buf_len,
                                         hazemon_rx_cmd_list_t *list);

static size_t __encode_var_size(uint64_t val, uint8_t *out_buf);
static size_t __decode_var_size(const uint8_t *buf, uint64_t *out_val);

static uint8_t __maxim_crc8(const uint8_t *data, size_t len);
/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

can5_err_t hazemon_make_tx_packet(const can5_sensor_data_list_t *list, uint8_t *out_pkt, size_t *out_len)
{
    TRACE_FUNC;
    can5_sensor_data_t *timestamp_data;

    timestamp_data = can5_sensor_data_get_timestamp(list);

    if (!timestamp_data) {
        return CAN5_CODEC_ERR_NO_TIMESTAMP;
    }

    // negative value
    if (timestamp_data->val[0] == '-') {
        return CAN5_NET_ERR_PARSE_INCOMPLETE;
    }

    return __make_hazemon_packet(list, timestamp_data, out_pkt, out_len);
}

can5_err_t hazemon_parse_rx_packet(const uint8_t *data, size_t buf_len,
                                   hazemon_rx_cmd_list_t *out_list)
{
    TRACE_FUNC;

    uint8_t crc = __maxim_crc8(data, buf_len - CRC_LEN);
    if (crc != data[buf_len - 1]) {
        return CAN5_CODEC_ERR_RX_INVALID_CRC;
    }
    return __parse_hazemon_packet(data, buf_len, out_list);
}

void hazemon_rx_cmd_list_free(hazemon_rx_cmd_list_t *list, bool free_list)
{
    TRACE_FUNC;

    hazemon_rx_cmd_t *cur, *next;

    TAILQ_FOREACH_SAFE(cur, list, te, next) {
        TAILQ_REMOVE(list, cur, te);
        free(cur);
    }

    if (free_list) {
        free(list);
    }

}

hazemon_type_t can5_map_get_hazemon_type(can5_sensor_data_type_t type)
{
    for (size_t i = 0; i < sizeof(__sensor_to_hazemon_type) / sizeof(__sensor_to_hazemon_type[0]); i++) {
        if (type == __sensor_to_hazemon_type[i].sensor_data_type) {
            return __sensor_to_hazemon_type[i].hazemon_type;
        }
    }

    return HAZEMON_NONE;
}

hazemon_datatype_t can5_map_get_hazemon_datatype(const char *datatype)
{
    if (strcmp(datatype, "string") == 0) {
        return DT_STRING;
    }

    if (strlen(datatype) == 1) {
        switch (datatype[0]) {
            case 'B':
                return DT_UNSIGNED_CHAR;
            case 'f':
                return DT_FLOAT;
            case 'h':
                return DT_SHORT;
            case 'H':
                return DT_UNSIGNED_SHORT;
            case 'L':
                return DT_UNSIGNED_LONG;
            case 'Q':
                return DT_UNSIGNED_LONG_LONG;
        }
    }

    return DT_INVALID;
}

size_t can5_map_get_expected_hazemon_datapoint_len(const can5_sensor_data_t *sensor_data, hazemon_datatype_t datatype)
{
    size_t ret = 0;
    switch (datatype) {
        case DT_UNSIGNED_CHAR:
            ret = sizeof(uint8_t);
            break;
        case DT_FLOAT:
            ret = sizeof(float);
            break;
        case DT_SHORT:
            ret = sizeof(int16_t);
            break;
        case DT_UNSIGNED_SHORT:
            ret = sizeof(uint16_t);
            break;
        case DT_STRING:
            if (sensor_data->datatype == CAN5_SENSOR_DATA_DATATYPE_STR) {
                ret = strlen(sensor_data->val);
            }
            break;
        case DT_UNSIGNED_LONG:
            ret = sizeof(uint32_t);
            break;
        case DT_UNSIGNED_LONG_LONG:
            ret = sizeof(uint64_t);
            break;
        case DT_INVALID:
            break;
    }

    return ret;
}
static size_t __encode_var_size(uint64_t val, uint8_t *out_buf)
{
    TRACE_FUNC;
    size_t len = 0;
    uint8_t *b_val = out_buf;
    void *var_val = out_buf + sizeof(uint8_t);

    /* this is not possible in c
    if (val < 0) {
        len = 0;

    }
    */

    if (val < 253) {

        *b_val = (uint8_t) val;
        len = sizeof(uint8_t);

    } else if (val < UINT16_MAX) { // 2 ** 16

        *b_val = 253;
        *(uint16_t *)var_val = (uint16_t )htole16(val);
        len = sizeof(uint8_t) + sizeof(uint16_t);

    } else if (val < UINT32_MAX) { // 2 ** 32

        *b_val = 254;
        *(uint32_t *)var_val = (uint32_t )htole32(val);
        len = sizeof(uint8_t) + sizeof(uint32_t);

    } else if (val < UINT64_MAX) {

        *b_val = 255;
        *(uint64_t *)var_val = (uint64_t )htole64(val);
        len = sizeof(uint8_t) + sizeof(uint64_t);
    }

    return len;
}

/**
 * @brief input buf and get out_val with size occupied in buf.
 * @param 8f b + sizeof(uint64_t)uffer container var size value
 * @param out_val output
 * @return  size of the var size in buffer
 */
static size_t __decode_var_size(const uint8_t *buf, uint64_t *out_val)
{
    TRACE_FUNC;
    uint8_t b_val = 0;
    size_t len;

    b_val = buf[0];
    len = 1;

    if (b_val < 253 ) {
        *out_val = b_val;

    } else if (b_val == 253) {
        *out_val = le16toh(*(uint16_t *)(buf + 1));
        len += sizeof (uint16_t);

    } else if (b_val == 254) {
        *out_val = le32toh(*(uint32_t *)(buf + 1));
        len += sizeof (uint32_t);

    } else if (b_val == 255) {
        *out_val = le64toh(*(uint64_t *)(buf + 1));
        len += sizeof (uint64_t);
    }

    return len;
}

/*
 * Make sure out_buf is atleast VAR_MAX_SIZE
 */
static can5_err_t __make_device_id_data(uint8_t *out_buf, size_t *out_len)
{
    TRACE_FUNC;
    int64_t device_id;
    int64_t project_id;

    VERIFY_SUCCESS(config_manager.read_int(CFG_DEVICE_ID, &device_id));
    VERIFY_SUCCESS(config_manager.read_int(CFG_DEVICE_ID_POSTFIX, &project_id));

    device_id = device_id * 10 + project_id;
    ESP_LOGI_V(TAG, "Hazemon id %llu %llx", device_id, device_id);

    *out_len = __encode_var_size(device_id, out_buf);

    if (!*out_len) {
        return CAN5_CODEC_ERR_VAL_PARSE;
    }

    return CAN5_SUCCESS;
}

static float __XXX_fix_float_val_hazemon_XXX__(hazemon_type_t type, float x)
{
    TRACE_FUNC;

    if (type == HAZEMON_CO || type == HAZEMON_MQ7_CO || type == HAZEMON_RAIN) {
        return (float) ((uint32_t) (x * 10));
    }

    return x;
}

static int64_t __adjust_int(int64_t data, hazemon_type_t type)
{
    return (int64_t) ((1 / can5_hazemon_get_type(type)->multiplier) * data);
}

static int64_t __adjust_float(float data, hazemon_type_t type)
{
    return (int64_t) ((1 / can5_hazemon_get_type(type)->multiplier) * data);
}

static can5_err_t __get_binary_data(can5_sensor_data_t *sensor_data,
                                  hazemon_type_t type, hazemon_datatype_t datatype, uint8_t *out_buf)
{
    TRACE_FUNC;
    bool success;
    // int64_t result_n;
    double result_d;
    char *result_s;

    switch (datatype) {

        case DT_UNSIGNED_CHAR:
            result_d = can5_sensor_data_get_dec(sensor_data, &success, false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            *(uint8_t *)out_buf = __adjust_float(result_d, type);
            break;

        case DT_FLOAT:
            result_d = can5_sensor_data_get_dec(sensor_data, &success, false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            *(float *)out_buf = __XXX_fix_float_val_hazemon_XXX__(type, (float)result_d);
            /*
             * The hazemon server is x86_64, so little endian,
             * and the float representation has to be in little endian.
             * Thankfully esp32 and the hazemon server both follows IEEE-754.
             */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            *(float *)out_buf = __bswap32(*(float *)data);
#endif
            break;

        case DT_SHORT:
            result_d = can5_sensor_data_get_dec(sensor_data, &success, false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            *(int16_t *)out_buf = htole16((int16_t) __adjust_float(result_d, type));
            break;

        case DT_UNSIGNED_SHORT:
            result_d = can5_sensor_data_get_dec(sensor_data, &success, false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            *(uint16_t *)out_buf = htole16((uint16_t) __adjust_float(result_d, type));
            break;

        case DT_STRING:
            result_s = can5_sensor_data_get_str(sensor_data, false);
            strcpy((char *)out_buf, result_s);
            free(result_s);
            break;

        case DT_UNSIGNED_LONG:
            result_d = can5_sensor_data_get_dec(sensor_data, &success, false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            *(uint32_t *)out_buf = htole32((uint32_t) __adjust_float(result_d, type));
            break;

        case DT_UNSIGNED_LONG_LONG:
            result_d = can5_sensor_data_get_dec(sensor_data, &success, false);
            if (!success) {
                return CAN5_CODEC_ERR_INVALID_DATATYPE;
            }

            *(uint64_t *)out_buf = htole64((uint64_t) __adjust_float(result_d, type));
            break;

        case DT_INVALID:
        default:
            return CAN5_CODEC_ERR_INVALID_DATATYPE;
    }

    return CAN5_SUCCESS;
}

static size_t __encode_sensor_data_list(const can5_sensor_data_list_t *list, uint8_t *out_buf)
{
    TRACE_FUNC;
    size_t pos;
    can5_sensor_data_t *sensor_data;

    pos = 0;

    TAILQ_FOREACH(sensor_data, list, te) {
        size_t cur_pos;     // position for each sensor value
        const hazemon_type_token_t *hazemon_token;
        hazemon_type_t hazemon_type;
        hazemon_datatype_t hazemon_datatype;
        size_t data_len;
        size_t type_buf_len, length_buf_len;

        cur_pos = 0;
        hazemon_type = can5_map_get_hazemon_type(sensor_data->type);

        if (hazemon_type == HAZEMON_NONE) {
            continue;
        }

        // skip timestamp in hazemon bytes
        if (hazemon_type == HAZEMON_TIMESTAMP) {
            continue;
        }

        // encode hazemon type
        hazemon_token = can5_hazemon_get_type(hazemon_type);
        hazemon_datatype = can5_map_get_hazemon_datatype(hazemon_token->datatype);

        type_buf_len = __encode_var_size(hazemon_type, &out_buf[pos + cur_pos]);

        if (!type_buf_len) {
            continue;
        }

        cur_pos += type_buf_len;

        // encode length of the value
        data_len = can5_map_get_expected_hazemon_datapoint_len(sensor_data, hazemon_datatype);

        if (!data_len) {
            continue;
        }

        length_buf_len = __encode_var_size(data_len, &out_buf[pos + cur_pos]);
        if (!length_buf_len) {
            continue;
        }

        cur_pos += length_buf_len;

        // encode the rel value
        if (__get_binary_data(sensor_data, hazemon_type, hazemon_datatype,
                              &out_buf[pos + cur_pos]) != CAN5_SUCCESS) {
            continue;
        }

        cur_pos += data_len;

        pos += cur_pos;
    }

    return pos;
}


static can5_err_t __make_hazemon_packet(const can5_sensor_data_list_t *list,
                                        const can5_sensor_data_t *time_sensor_data,
                                        uint8_t *out_pkt,
                                        size_t *out_len)
{
    TRACE_FUNC;
    size_t len;
    size_t total_len;
    size_t pkt_len_pos;
    uint64_t _device_timestamp;
    bool sd_success;


    len = total_len = pkt_len_pos = 0;

    //1. VL(device_id) -> VL(device_id * 10 + project_id) : ones digit should be project id
    VERIFY_SUCCESS(__make_device_id_data(out_pkt, &len));

    total_len += len;

    //2. timestamp:uint64_t
    /* type punning */

    _device_timestamp = can5_sensor_data_get_num(time_sensor_data, &sd_success, true);

    if (!sd_success) {
        return CAN5_CODEC_ERR_VAL_PARSE;
    }

    *(uint64_t *)&out_pkt[total_len] = htole64(_device_timestamp);

    total_len += sizeof(_device_timestamp);

    //3. length(packet):uint16_t -- Get length later

    pkt_len_pos = total_len;

    total_len += sizeof(uint16_t);

    //4. datapoints ... VL(type) + VL(length(data)) + data

    len = __encode_sensor_data_list(list, &out_pkt[total_len]);
    // if timestamp is 0, it means hello packet, continue processing
    if (!len && _device_timestamp != 0){
        return CAN5_CODEC_ERR_VAL_PARSE;
    }

    total_len += len;

    // go back to getting length in 3.
    *(uint16_t *)&out_pkt[pkt_len_pos] = htole16(total_len + CRC_LEN);

    // calculate crc
    out_pkt[total_len] = __maxim_crc8(out_pkt, total_len);

    total_len += CRC_LEN;

    *out_len = total_len;

    return CAN5_SUCCESS;
}

#define CHECK_RX_LEN()  if (buf_len < pos + expected_block_len) return CAN5_CODEC_ERR_RX_PARSE

static void __set_datapoint_val(hazemon_rx_cmd_t *cmd, const uint8_t *buf, hazemon_datatype_t datatype, size_t len)
{
    TRACE_FUNC;

    switch (datatype) {

        case DT_UNSIGNED_CHAR:
            sprintf(cmd->val, "%u", buf[0]);
            break;

        case DT_FLOAT:
            sprintf(cmd->val, "%f", *((float *) buf));
            break;

        case DT_SHORT:
            sprintf(cmd->val, "%d" , le16toh(*((int16_t *)buf)));
            break;

        case DT_UNSIGNED_SHORT:
            sprintf(cmd->val, "%u", le16toh(*((uint16_t *)buf)));
            break;

        case DT_STRING:
            strncpy(cmd->val, (char *)buf, len);
            break;

        case DT_UNSIGNED_LONG:
            sprintf(cmd->val, "%u", le32toh(*((uint32_t *)buf)));
            break;

        case DT_UNSIGNED_LONG_LONG:
            sprintf(cmd->val , "%llu", le64toh(*((uint64_t *)buf)));
            break;

        case DT_INVALID:
        default:
            break;
    }
}

static can5_err_t __decode_sensor_data(const uint8_t *buf, size_t buf_len,
                                       hazemon_rx_cmd_list_t *list, size_t *dlist_len)
{
    TRACE_FUNC;

    can5_err_t ret;
    uint64_t t_val , l_val;
    size_t pos, t_len, l_len;
    const hazemon_type_token_t *hazemon_type_token;
    hazemon_datatype_t hazemon_datatype;
    hazemon_rx_cmd_t *cmd;

    pos = 0;

    while (pos < buf_len) {
        /* tlv processing */
        t_val = l_val = 0;
        t_len = l_len = 0;


        // 6.1 decode type
        t_len = __decode_var_size(&buf[pos], &t_val);
        if (!t_len) {
            // TODO: optimistic break, discard packet if this is invalid
            ret = CAN5_CODEC_ERR_RX_PARSE;
            goto error;
        }
        pos += t_len;

        // 6.2 decode length
        l_len = __decode_var_size(&buf[pos], &l_val);
        if (!l_len) {
            // TODO: optimistic break, discard packet if this is invalid
            ret = CAN5_CODEC_ERR_RX_PARSE;
            goto error;
        }
        pos += l_len;

        // 6.3 decode value
        if (!(hazemon_type_token = can5_hazemon_get_type(t_val))) {
            ret =  CAN5_CODEC_ERR_RX_PARSE;
            goto error;
        }

        VERIFY_ALLOC_SAFENORETURN(cmd, sizeof(hazemon_rx_cmd_t), {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto error;
        });

        cmd->type = hazemon_type_token->type;

        hazemon_datatype = can5_map_get_hazemon_datatype(hazemon_type_token->datatype);

        __set_datapoint_val(cmd, &buf[pos], hazemon_datatype, l_val);
        pos += l_val;

        TAILQ_INSERT_TAIL(list, cmd, te);
    }

    *dlist_len = pos;

    return CAN5_SUCCESS;

error:
    hazemon_rx_cmd_list_free(list, false);
    return ret;
}

static can5_err_t __parse_hazemon_packet(const uint8_t *data, size_t buf_len,
                                         hazemon_rx_cmd_list_t *list)
{
    TRACE_FUNC;
    uint8_t *buf;
    size_t pos, device_id_len, dlist_len, expected_block_len;
    uint64_t device_id;
    uint16_t pkt_len;
    uint64_t server_timestamp;
    int64_t  cfg_device_id, cfg_project_id;
    time_t now;

    buf = (uint8_t *)data;
    pos = 0;


    // 1. first byte should be 0
    expected_block_len = 1;
    CHECK_RX_LEN();

    if (buf[pos] != 0x00) {
        return CAN5_CODEC_ERR_RX_PARSE;
    }

    pos += 1;

    // 2. Get device id as VL

    expected_block_len = 1;
    CHECK_RX_LEN();

    device_id_len = __decode_var_size(&buf[pos], &device_id);

    if (!device_id_len) {
        return CAN5_CODEC_ERR_RX_PARSE;
    }

    // 3. Match device id
    VERIFY_SUCCESS(config_manager.read_int(CFG_DEVICE_ID, &cfg_device_id));
    VERIFY_SUCCESS(config_manager.read_int(CFG_DEVICE_ID_POSTFIX, &cfg_project_id));

    cfg_device_id = cfg_device_id * 10 + cfg_project_id;

    if (device_id != cfg_device_id) {
        return CAN5_CODEC_ERR_RX_PARSE;
    }
    pos += device_id_len;

    // 4. Get timestamp

    expected_block_len = sizeof(server_timestamp);
    CHECK_RX_LEN();

    server_timestamp = le64toh(*(uint64_t *)(&buf[pos]));

    now = time(NULL);
    if (now == -1) {
        return CAN5_TIME_ERR_GET_SYS_TIME;
    }

#if CONFIG_CAN5_NET_SYNC_HAZEMON_SERVER
    #pragma message("Time Sync with Hazemon server enabled.")
    if (abs((time_t)server_timestamp - now) > SERVER_TIME_THRESHOLD_SEC) {
        struct timeval server_tv;
        server_tv.tv_sec = (time_t)server_timestamp;
        server_tv.tv_usec = 0;
        if (settimeofday(&server_tv, NULL) != 0) {
            return CAN5_TIME_ERR_SET_SYS_TIME;
        }
        VERIFY_SUCCESS(rtc.sys_to_rtc());
    }
#endif

    pos += sizeof(server_timestamp);

    // 7. check length
    expected_block_len = sizeof(pkt_len);
    CHECK_RX_LEN();

    pkt_len = le16toh(*(uint16_t *)(&buf[pos]));

    if (pkt_len != buf_len) {
        return CAN5_CODEC_ERR_RX_PARSE;
    }

    pos += sizeof(pkt_len);

    // 6. Check for config
    if (pos + CRC_LEN == buf_len) {
        // we don't have config
        return CAN5_SUCCESS;
    }

    // 7. Parse config
    dlist_len = 0;
    VERIFY_SUCCESS(__decode_sensor_data(&buf[pos], buf_len - pos - CRC_LEN, list, &dlist_len));
    pos += dlist_len;

    if (pos + CRC_LEN == buf_len) {
        return CAN5_SUCCESS;
    }

    if (list) {
        //free_list_datapoint(*dlist);
    }

    return CAN5_CODEC_ERR_RX_PARSE;
}

hazemon_type_t __get_hazemon_type(can5_sensor_data_type_t type)
{
    for(size_t i = 0; i < sizeof(__sensor_to_hazemon_type)/sizeof(__sensor_to_hazemon_type[0]); i++){
        if (type == __sensor_to_hazemon_type[i].sensor_data_type) {
            return __sensor_to_hazemon_type[i].hazemon_type;
        }
    }

    return HAZEMON_NONE;
}

/* ---------------------------------------------------------------------
 * DOW/Maxim CRC8 used by Hazemon server
 -----------------------------------------------------------------------*/
/* Check    Poly    Init    RefIn   RefOut  XorOut
 * xA1      0x31    0x00    true    true    0x00
 *
 * Esp32's ROM table uses 0x07 as the polynomial which is incompatible.
 */

static const uint8_t __maxim_table[] = {
    0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
    0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
    0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e,
    0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
    0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0,
    0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
    0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d,
    0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
    0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5,
    0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
    0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58,
    0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
    0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6,
    0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
    0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b,
    0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
    0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f,
    0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
    0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92,
    0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
    0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c,
    0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
    0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1,
    0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
    0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49,
    0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
    0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4,
    0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
    0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a,
    0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
    0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7,
    0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35
};

static uint8_t __maxim_crc8(const uint8_t *data, size_t len)
{
    TRACE_FUNC;
    uint8_t init = 0;
    for (size_t i = 0; i < len; i++) {
        init = __maxim_table[(init & 0xFF) ^ (data[i] & 0xFF)];
    }
    return init;
}
