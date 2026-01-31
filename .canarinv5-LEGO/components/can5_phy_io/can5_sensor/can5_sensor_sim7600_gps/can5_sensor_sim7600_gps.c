/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.c
* Descr:
*******************************************************************************/

#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_sensor_sim7600_gps.h"
#include "can5_hazemon_types.h"
#include "parse.h"

#define TAG "SENSOR_SIM7600_GPS"

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_SIM7600_GPS_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI_V(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_sim7600_status_e {
    SENSORDRIV_SIM7600_GPS_STAT_UNINITD = 0,
    SENSORDRIV_SIM7600_GPS_STAT_INITD,
    SENSORDRIV_SIM7600_GPS_STAT_READING,
} sensordriv_sim7600_status_t;

static can5_sensor_hdl_t *alloc(size_t *len);

static uint8_t get_sensor_id(can5_sensor_hdl_t *hdl);

static void set_sensor_id(can5_sensor_hdl_t *hdl, uint8_t sensor_id);

static can5_err_t detect(can5_sensor_hdl_t *hdl, can5_port_idx_t port);

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb);

static can5_err_t uninit(can5_sensor_hdl_t *hdl);

static can5_err_t run(can5_sensor_hdl_t *hdl);

static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t timeout, bool blocking);

static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f *);

static can5_err_t enable(can5_sensor_hdl_t *hdl, bool enable);

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len);

static bool is_running(can5_sensor_hdl_t *hdl);

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response);

static int32_t status_get(can5_sensor_hdl_t *hdl);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status);

#endif


const can5_sensordriv_t sensordriv_sim7600_gps = {
    .ops = {
        .alloc = alloc,
        .get_id = get_sensor_id,
        .set_id = set_sensor_id,
        .detect = detect,
        .init = init,
        .uninit = uninit,
        .run = run,
        .read = read,
        .register_read_cb = register_read_cb,
        .enable = enable,
        .get_sensor_data = get_sensor_data,
        .is_running = is_running,
        .driverctl = driverctl,
        .status_get = status_get,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0
        .status_getstr = status_getstr,
#endif
    },
    .details = {
        .sensor = {
            .init_warm_up = CAN5_SENSOR_TIME_MIN,
            .read_warm_up = CAN5_SENSOR_TIME_MIN,
            .read_time = CAN5_SENSOR_TIME_MIN,
            .type = CAN5_SENSORDRIV_TYPE_SIM7600_GPS,
        },
        .io_type = CAN5_PHY_IO_TYPE_I2C,
        .name = "Sim7600",
        .version = "1.0",
        .manufacturer = "SimCom",
    }
};

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_sim7600_status_t status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_sim7600_gps_data_t cb_data;
    char out_str[128];
    can5_sensor_hwcb_f *hwcb;
} sensor_hdl_t;


#define SENSOR_HW_EVT(hdl, evt_type)                \
do {                                                \
    sensor_hdl_t *ss_hdl = (hdl)->hdl;       \
    if (ss_hdl->hwcb) {                             \
        can5_sensor_hw_evt_t evt = {                \
            .type = (evt_type),                     \
            .sensor_id = get_sensor_id(hdl),        \
        };                                          \
        ss_hdl->hwcb(&evt);                         \
    }                                               \
} while (0)

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t __parse(const char *str, can5_sensor_sim7600_gps_data_t *data);

/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/
static can5_sensor_hdl_t *alloc(size_t *len)
{
    can5_sensor_hdl_t *hdl;
    sensor_hdl_t *s_hdl;
    *len = sizeof(can5_sensor_hdl_t) + sizeof(sensor_hdl_t);

    hdl = malloc(*len);
    if (!hdl) {
        return NULL;
    }

    s_hdl = hdl->hdl = hdl->hdl_data;

    CLEAR_STRUCT(*s_hdl);
    s_hdl->port = CAN5_PORT_NULL;
    s_hdl->sensor_id = CAN5_SENSOR_ID_NONE;
    s_hdl->status = SENSORDRIV_SIM7600_GPS_STAT_UNINITD;

    return hdl;
}

static uint8_t get_sensor_id(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->sensor_id;
}

static void set_sensor_id(can5_sensor_hdl_t *hdl, uint8_t sensor_id)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    s_hdl->sensor_id = sensor_id;
}

static can5_err_t detect(can5_sensor_hdl_t *hdl, can5_port_idx_t port)
{
    TRACE_FUNC;

    if (hal.get_cell_rssi() == 0) {
        return CAN5_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "SimCom7600 GPS detected!");

    return CAN5_SUCCESS;
}

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    s_hdl->hwcb = hwcb;
    s_hdl->port = port;

    s_hdl->status = SENSORDRIV_SIM7600_GPS_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_SIM7600_GPS_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        // read here
        can5_sensor_sim7600_gps_data_t *gps_data;
        CLEAR_STRUCT(s_hdl->cb_data);
        gps_data = &s_hdl->cb_data;

        s_hdl->async_read = false;
        //hal.analog_read(&gps_data->val, s_hdl->port);
        hal.get_cell_gps(s_hdl->out_str);
        __parse(s_hdl->out_str, &s_hdl->cb_data);
        s_hdl->status = SENSORDRIV_SIM7600_GPS_STAT_INITD;
        // callback read data here
        s_hdl->readcb(get_sensor_id(hdl), gps_data, sizeof (*gps_data));
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);
    }

    return CAN5_SUCCESS;
}

/**
 * @brief Read sensor data
 * @param prxdata  buffer to read data into, can be null if blocking is false
 * @param plen  size to write to, can be null if blocking is false
 * @param blocking if this function should block or dispatch read
 */
static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t timeout, bool blocking)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;


    CHECK_INITD(s_hdl);
    if (s_hdl->status == SENSORDRIV_SIM7600_GPS_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_SIM7600_GPS_STAT_READING;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_SIM7600_GPS_STAT_INITD;
        return CAN5_ERR_UNIMPLEMENTED;
    }
    else {
        // dispatch the read to run() and call read_cb when done
        s_hdl->async_read = true;
    }
    return CAN5_SUCCESS;
}

static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f* cb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    VERIFY_NOT_NULL(cb);
    s_hdl->readcb = cb;

    return CAN5_SUCCESS;
}

static can5_err_t enable(can5_sensor_hdl_t *hdl, bool enable)
{
    /* not applicable */
    return CAN5_SUCCESS;
}

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len)
{
    sensor_hdl_t *s_hdl = hdl->hdl;
    const can5_sensor_sim7600_gps_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (sizeof(can5_sensor_sim7600_gps_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    if (s_data->flags & CAN5_GPS_LAT) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->lat);

        if (elem) {
            TAILQ_INSERT_TAIL(list, elem, te);
        }
    }


    if (s_data->flags & CAN5_GPS_LNG) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->lng);
        if (elem) {
            TAILQ_INSERT_TAIL(list, elem, te);
        }
    }


    if (s_data->flags & CAN5_GPS_ALT) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->alt);
        if (elem) {
            TAILQ_INSERT_TAIL(list, elem, te);
        }
    }

done:
    return list;
}

static bool is_running(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->status >= SENSORDRIV_SIM7600_GPS_STAT_INITD;
}


static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request,const void *params, void *response)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    switch (request) {
        default:
            break;
    }

    return CAN5_SUCCESS;
}

static int32_t status_get(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->status;
}

/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/

static can5_err_t __parse(const char *in_str, can5_sensor_sim7600_gps_data_t *data)
{
    typedef enum tokens_e {
        MODE = 0,
        GPS_SV,
        GLONASS_SV,
        BEIDOU_SV,
        LAT,
        N_S,
        LOG,
        E_W,
        DATE,
        UTC_TIME,
        ALT,
        MAX,
    } tokens_t;

    char *str, *token, *r_ptr;
    tokens_t curr_token;

    if (!strlen(in_str)) {
        return CAN5_ERR_INVALID_PARAM;
    }
    r_ptr = str = strdup(in_str);
    token = strtok_r(str, ",", &r_ptr);
    curr_token = MODE;

    while(token) {
        nmea_position pos;
        nmea_cardinal_t cardinal;
        double decimal;
        remove_spaces(token);

        // process tokens here
        switch (curr_token) {

            case MODE:
                data->fix = true;
                break;

            case GPS_SV:
                data->n_sat_active += strtol(token, NULL, 10);
                break;

            case GLONASS_SV:
                data->n_sat_active += strtol(token, NULL, 10);
                break;

            case BEIDOU_SV:
                data->n_sat_active += strtol(token, NULL, 10);
                break;

            case LAT:
                CLEAR_STRUCT(pos);
                if (!nmea_position_parse(token, &pos)) {
                    decimal = pos.degrees + pos.minutes / 60;
                    data->lat = decimal;
                }
                break;

            case N_S:
                cardinal = nmea_cardinal_direction_parse(token);
                if (cardinal == NMEA_CARDINAL_DIR_NORTH) {
                    data->flags |= CAN5_GPS_LAT;

                }
                else if (cardinal == NMEA_CARDINAL_DIR_SOUTH) {
                    data->flags |= CAN5_GPS_LAT;
                    data->lat *= -1;
                }
                else {
                    data->fix = false;
                }
                break;

            case LOG:
                CLEAR_STRUCT(pos);
                if (!nmea_position_parse(token, &pos)) {
                    decimal = pos.degrees + pos.minutes / 60;
                    data->lng = decimal;
                }
                break;

            case E_W:
                cardinal = nmea_cardinal_direction_parse(token);
                if (cardinal == NMEA_CARDINAL_DIR_EAST) {
                    data->flags |= CAN5_GPS_LNG;

                }
                else if (cardinal == NMEA_CARDINAL_DIR_WEST) {
                    data->lng *= -1;
                    data->flags |= CAN5_GPS_LNG;
                }
                else {
                    data->fix = false;
                }
                break;

            case DATE:
                break;

            case UTC_TIME:
                break;

            case ALT:
                data->alt = atof(token);
                data->flags |= CAN5_GPS_ALT;
                break;

            case MAX:
                break;
        }


        token = strtok_r(NULL, ",", &r_ptr);
        curr_token++;
    }

    free(str);

    return CAN5_SUCCESS;
}

/* ---------------------------------------------------------------------
 * Debug Support
 -----------------------------------------------------------------------*/

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief NEtif Status tags
 *
 */
static const can5_tag_tab_t _SENSORDRIV_SIM7600_GPS_STAT_tags = {
        TAG_TAB_ITEM(SENSORDRIV_SIM7600_GPS_STAT_UNINITD),
        TAG_TAB_ITEM(SENSORDRIV_SIM7600_GPS_STAT_INITD),
        TAG_TAB_ITEM(SENSORDRIV_SIM7600_GPS_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_SIM7600_GPS_STAT_tags);
}

#endif
void parse_sim7600_gps_read_data(const can5_sensordriv_t *driv, const can5_sensor_sim7600_gps_data_t *gps_data, void *pout, size_t *plen)
{
    if (!gps_data->fix) return;
    size_t len;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    len = 0;

    if ((gps_data->flags & (CAN5_GPS_LAT | CAN5_GPS_LNG)) == (CAN5_GPS_LAT | CAN5_GPS_LNG)) {
        len += sprintf(&((char *)pout)[len], "%s:%lf,%s:%lf",
                       can5_hazemon_get_type(HAZEMON_GPS_LAT)->token, gps_data->lat,
                       can5_hazemon_get_type(HAZEMON_GPS_LNG)->token, gps_data->lng);
    }

    if (gps_data->flags & CAN5_GPS_ALT) {
        sprintf(&((char *)pout)[len], ",%s:%lf",
                can5_hazemon_get_type(HAZEMON_GPS_ALT)->token, gps_data->alt);

    }

    *plen = strlen(pout);
}
