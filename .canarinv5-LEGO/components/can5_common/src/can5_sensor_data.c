/**************************************************
 * Author: rmukhia
 * Creation Date: 21/7/22
 * Description: 
 **************************************************/
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <stdlib.h>
#include "can5_sensor_data.h"
#include "can5_utils.h"

static const char *SENSOR_DATA_TYPE_PREFIX = "CAN5_SENSOR_DATA_TYPE_";

//#define STRICT_CHECKING


static const struct {
    const char *str;
    can5_sensor_data_datatype_t datatype;
} datatype_to_str[] = {
    { "n" , CAN5_SENSOR_DATA_DATATYPE_NUM },
    { "d" , CAN5_SENSOR_DATA_DATATYPE_DEC },
    { "s" , CAN5_SENSOR_DATA_DATATYPE_STR },
};

static const struct {
    const char *str;
    can5_port_idx_t port;
} port_to_str[] = {
        { "none" , CAN5_PORT_NULL },
        { "adc0" , ADPORT_0 },
        { "adc1" , ADPORT_1 },
        { "adc2" , ADPORT_2 },
        { "adc3" , ADPORT_3 },
        { "i2c"  , I2C_0    },
        { "uart0", UPORT_0  },
        { "uart1", UPORT_1  },
        { "uart2", UPORT_2  },
        { "uart3", UPORT_3  },
        { "uart4", UPORT_4  },
        { "uart5", UPORT_5  },
        { "uart6", UPORT_6  },
        { "uart7", UPORT_7  },
};


// get enum from string
static const can5_tag_tab_t __sensor_data_type_tags = {
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_TIMESTAMP),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_NUM_CYCLE_DATA),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_NORTH),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_EAST),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_DOWN),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_CF1),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_CF1),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_CF1),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_ATM),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_ATM),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_ATM),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MH_Z16_CO2),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_ZE03_NO2),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_ZE07_CO),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_BME280_TEMP),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_BME280_PRES),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_BME280_HUMI),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_WS3226_RAIN),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_PERCENTAGE),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_X),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Y),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Z),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_X),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Y),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Z),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_SCD41_CO2),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_SCD41_HUMI),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_TYPE_SCD41_TEMP),
};

static const can5_sensor_data_type_t  __gps_type_list[] = {
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT,
    CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT,
    CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG,
    CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT,
    // add more here
};

bool can5_sensor_data_is_gps_type(can5_sensor_data_type_t type) {
    for (int i = 0; i < sizeof(__gps_type_list)/ sizeof (__gps_type_list[0]); i++) {
        if (type == __gps_type_list[i]){
            return true;
        }
    }
    return false;
}

const char *can5_sensor_data_type_getstr(can5_sensor_data_type_t type)
{
    return TAG_LOOKUP(type, __sensor_data_type_tags);
}

static const can5_tag_tab_t __sensor_data_datatype_tags = {
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_DATATYPE_NUM),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_DATATYPE_DEC),
    TAG_TAB_ITEM(CAN5_SENSOR_DATA_DATATYPE_STR),
};

const char *can5_sensor_data_datatype_getstr(can5_sensor_data_datatype_t type)
{
    return TAG_LOOKUP(type, __sensor_data_datatype_tags);
}

int tag_reverse_lookup(const char *val, can5_tag_tab_t lut, size_t lut_size) {
    for (size_t t=0; t<lut_size/sizeof(can5_tag_item_t); t++) {
        if (strcmp(lut[t].tag, val) == 0) return lut[t].code;
    }
    return -1;
}

size_t can5_sensor_data_dumps(const can5_sensor_data_t *sensor_data, char *out_str)
{
    char *sensor_type;
    const char *sensor_port;
    const char *sensor_datatype;

    sensor_datatype = NULL;

    for (size_t i = 0; i < sizeof(datatype_to_str)/sizeof(datatype_to_str[0]); i++) {
        if (sensor_data->datatype == datatype_to_str[i].datatype) {
            sensor_datatype = datatype_to_str[i].str;
            break;
        }
    }

    if (!sensor_datatype) {
        return 0;
    }

    sensor_type = (char *) can5_sensor_data_type_getstr(sensor_data->type);
    if (!sensor_type) {
        return 0;
    }

    sensor_type += strlen(SENSOR_DATA_TYPE_PREFIX);

    // set to none
    sensor_port = port_to_str[0].str;

    for(size_t i = 0; i < sizeof(port_to_str) / sizeof(port_to_str[0]); i++) {
        if (sensor_data->port == port_to_str[i].port) {
            sensor_port = port_to_str[i].str;
            break;
        }
    }

    return sprintf(out_str, "%s:%s:%s:%s", sensor_type, sensor_port, sensor_datatype, sensor_data->val);
}

typedef struct type_char_val_s {
    char *str;
    can5_sensor_data_type_t type;
} type_char_val_t;

size_t can5_sensor_data_list_dumps(const can5_sensor_data_list_t *sensor_data_list, char *out_str)
{
    can5_sensor_data_t *sensor_data;
    size_t pos;
    bool first;
    size_t found_elems, total_items;

    pos = 0;
    first = true;

    found_elems = 0;

    for (can5_sensor_data_type_t type = 0; type < CAN5_SENSOR_DATA_TYPE_COUNT; type++)
    {
        total_items = 0;
        TAILQ_FOREACH(sensor_data, sensor_data_list, te) {
            if (type == sensor_data->type) {
                if (!first) {
                    out_str[pos++] = ',';
                    out_str[pos++] = ' ';
                }
                pos += can5_sensor_data_dumps(sensor_data, &out_str[pos]);
                first = false;
                found_elems++;
            }
            total_items++;
        }

        if (total_items == found_elems) {
            break;
        }
    }

    out_str[pos] = '\0';

    return pos;
}

can5_sensor_data_t *can5_sensor_data_loads(const char *in_str)
{
    char *str, *token, *r_ptr;
    char type[64];
    char port_str[6];
    char data_type[3];
    char val[128];
    can5_sensor_data_t *sensor_data;
    enum token_type {
        _TYPE = 0,
        _PORT,
        _DATA_TYPE,
        _VAL,
    } token_type;

    token_type = _TYPE;
    r_ptr = str = strdup(in_str);
    token = strtok_r(str, ":", &r_ptr);

    while (token) {
        remove_spaces(token);

        switch (token_type) {

            case _TYPE:
                strcpy(type, SENSOR_DATA_TYPE_PREFIX);
                strcat(type, token);
                break;

            case _PORT:
                strcpy(port_str, token);
                break;

            case _DATA_TYPE:
                strcpy(data_type,  token);
                break;

            case _VAL:
                strcpy(val, token);
                break;

            default:
                break;
        }

        token_type++;

        token = strtok_r(NULL, ":", &r_ptr);
    }

    free(str);

    if (token_type < _VAL) {
        return NULL;
    }

    sensor_data = calloc(1, sizeof (can5_sensor_data_t));
    if (!sensor_data) {
        goto error;
    }

    sensor_data->datatype = -1;
    for(size_t i = 0; i < sizeof(datatype_to_str)/sizeof(datatype_to_str[0]); i++) {
        if (strcmp(data_type, datatype_to_str[i].str) == 0) {
            sensor_data->datatype = datatype_to_str[i].datatype;
            break;
        }
    }

    if (sensor_data->datatype == -1) {
        goto error;
    }

    sensor_data->type = tag_reverse_lookup(type,
                                           __sensor_data_type_tags,
                                           sizeof(__sensor_data_type_tags));
    if (sensor_data->type == -1) {
        goto error;
    }

    sensor_data->port = CAN5_PORT_NULL;
    for (size_t i = 0; i < sizeof (port_to_str) / sizeof (port_to_str[0]); i++) {
        if (strcmp(port_to_str[i].str, port_str) == 0) {
            sensor_data->port = port_to_str[i].port;
            break;
        }
    }

    sensor_data->val = strdup(val);
    if (!sensor_data->val) {
        goto error;
    }

    return sensor_data;

error:

    if (sensor_data) {
        can5_sensor_data_free(sensor_data);
    }

    return NULL;
}

can5_err_t can5_sensor_data_list_loads(const char *in_str, can5_sensor_data_list_t *list)
{
    can5_sensor_data_t *sensor_data;
    char *str, *token, *r_ptr;

    if (!strlen(in_str)) {
        return CAN5_ERR_INVALID_PARAM;
    }

    VERIFY_NOT_NULL(list);

    r_ptr = str = strdup(in_str);
    token = strtok_r(str, ",", &r_ptr);

    while (token) {
        remove_spaces(token);

        sensor_data = can5_sensor_data_loads(token);

        if (sensor_data) {
            TAILQ_INSERT_TAIL(list, sensor_data, te);
        }

        token = strtok_r(NULL, ",", &r_ptr);
    }

    free(str);

    return CAN5_SUCCESS;
}

void can5_sensor_data_free(can5_sensor_data_t *sensor_data)
{
    if (sensor_data) {
        if (sensor_data->val) {
            free(sensor_data->val);
        }
        free(sensor_data);
    }
}

void can5_sensor_data_list_free(can5_sensor_data_list_t *sensor_data_list)
{
    can5_sensor_data_t *cur, *next;
    TAILQ_FOREACH_SAFE(cur, sensor_data_list, te, next) {
        TAILQ_REMOVE(sensor_data_list, cur, te);
        can5_sensor_data_free(cur);
    }
}

static const char *dtype_fmt[] = {
    "%lli",     // number
    "%lf",      // decimal
    "%s"        // string
};

can5_sensor_data_t *can_5_sensor_data_create(can5_sensor_data_type_t type, can5_port_idx_t port,
                                             can5_sensor_data_datatype_t datatype,
                                             const char *str_val, int64_t num_val, double dec_val)
{
    can5_sensor_data_t *data;
    size_t val_len;
    const char *fmt;

    data = NULL;
    val_len = 0;

    data = calloc(sizeof(can5_sensor_data_t), 1);
    if (!data) {
        goto error;
    }

    data->type = type;
    data->datatype = datatype;
    data->port = port;
    data->val = NULL;

    switch (datatype) {

        case CAN5_SENSOR_DATA_DATATYPE_NUM:
            fmt = dtype_fmt[0];
            val_len = snprintf(NULL, 0, fmt, num_val);
            data->val = malloc(val_len + 1);
            if (!data->val) {
                goto error;
            }
            snprintf(data->val, val_len + 1, fmt, num_val);
            break;
        case CAN5_SENSOR_DATA_DATATYPE_DEC:
            fmt = dtype_fmt[1];
            val_len = snprintf(NULL, 0, fmt, dec_val);
            data->val = malloc(val_len + 1);
            if (!data->val) {
                goto error;
            }
            snprintf(data->val, val_len + 1, fmt, dec_val);
            break;
        case CAN5_SENSOR_DATA_DATATYPE_STR:
            fmt = dtype_fmt[2];
            val_len = snprintf(NULL, 0, fmt, str_val);
            data->val = malloc(val_len + 1);
            if (!data->val) {
                goto error;
            }
            snprintf(data->val, val_len + 1, fmt, str_val);
            break;
        case CAN5_SENSOR_DATA_DATATYPE_COUNT:
        default:
            goto error;
    }

    return data;

error:
    if (data) {
        if (data->val) {
            free(data->val);
        }
        free(data);
    }
    return NULL;
}

int64_t can5_sensor_data_get_num(const can5_sensor_data_t *data, bool *success, bool check_datatype)
{
    int64_t result = 0;
    char *endptr;

    if (check_datatype && data->datatype != CAN5_SENSOR_DATA_DATATYPE_NUM) {
        *success = false;
        return 0;
    }

    result = strtoll(data->val, &endptr,10);

    if (data->val == endptr) {
        *success = false;
        return 0;
    }

    *success = true;
    return result;
}

double can5_sensor_data_get_dec(const can5_sensor_data_t *data, bool *success, bool check_datatype)
{
    double result = 0;
    char *endptr;

    if (check_datatype && data->datatype != CAN5_SENSOR_DATA_DATATYPE_DEC) {
        *success = false;
        return 0;
    }

    result = strtod(data->val, &endptr);

    if (data->val == endptr) {
        *success = false;
        return 0;
    }

    *success = true;
    return result;
}

// TODO: make this static
char * can5_sensor_data_get_str(const can5_sensor_data_t *data, bool check_datatype)
{

    if (check_datatype && data->datatype != CAN5_SENSOR_DATA_DATATYPE_STR) {
        return NULL;
    }

    return strdup(data->val);
}

can5_sensor_data_t *can5_sensor_data_get_timestamp(const can5_sensor_data_list_t *list)
{
    can5_sensor_data_t *cur;

    TAILQ_FOREACH(cur, list, te) {
        if (cur->type == CAN5_SENSOR_DATA_TYPE_TIMESTAMP) {
            return cur;
        }
    }

    return NULL;
}


const char * can5_sensor_data_get_port_simple_str(can5_port_idx_t port)
{
    const char *val;

    val = port_to_str[0].str;

    for(size_t i = 0; i < sizeof(port_to_str) / sizeof(port_to_str[0]); i++) {
        if (port == port_to_str[i].port) {
            val = port_to_str[i].str;
            break;
        }
    }

    return val;
}