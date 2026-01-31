/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.c
* Descr:
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_rtc.h"
#include "can5_sensor_ublox_neo_gps.h"
#include "can5_hazemon_types.h"
#include "can5_config_provider.h"
#include "u_ubx_protocol.h"

static const char *TAG = "SENSOR_UBLOX_NEO_M8M";

#define UBLOX_NEO_START_BYTE1   0xB5
#define UBLOX_NEO_START_BYTE2   0x62


#define UBX_MAX_LEN           256


#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_UBLOX_NEO_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#define SERIAL_TIMEOUT              500

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_ublox_neo_status_e {
    SENSORDRIV_UBLOX_NEO_STAT_UNINITD = 0,
    SENSORDRIV_UBLOX_NEO_STAT_INITD,
    SENSORDRIV_UBLOX_NEO_STAT_READING,
} sensordriv_ublox_neo_status_t;

typedef struct __attribute__((__packed__)) ubx_mon_ver_s {
    char sw_version[30];
    char hw_version[10];
    char extenstion[30];
} ubx_mon_ver_t;

typedef struct __attribute__((__packed__)) ubx_nav_posllh_s {
    uint32_t i_tow;
    int32_t lon;
    int32_t lat;
    int32_t height;
    int32_t height_mean_sea_level;
    uint32_t h_acc;
    uint32_t v_acc;
} ubx_nav_posllh_t;

typedef struct __attribute__((__packed__)) ubx_nav_pvt_s {
    uint32_t i_tow;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
#define PVT_VALID_DATE          BIT(0)
#define PVT_VALID_TIME          BIT(1)
#define PVT_FULLY_RESOLVED      BIT(2)
#define PVT_VALID_MAG           BIT(3)
    uint8_t valid;
    uint32_t t_acc;
    int32_t nano;
    uint8_t fix_type;
    uint8_t flags;
    uint8_t flags2;
    uint8_t num_sv;
    int32_t lon;
    int32_t lat;
    int32_t height;
    int32_t height_mean_sea_level;
    uint32_t h_acc;
    uint32_t v_acc;
    int32_t vel_n;
    int32_t vel_e;
    int32_t vel_d;
    int32_t g_speed;
    int32_t head_mot;
    uint32_t s_acc;
    uint32_t head_acc;
    uint16_t p_dop;
    uint16_t flags3;
    uint8_t reserved1[4];
    int32_t head_veh;
    int16_t mag_dec;
    int16_t mag_acc;
} ubx_nav_pvt_t;

typedef union ubx_nav_u {
    ubx_nav_pvt_t pvt;
    ubx_nav_posllh_t posllh;
} ubx_nav_t;

typedef struct __attribute__((__packed__)) ubx_cfg_prt_s {
    uint8_t port_id;
    uint8_t reserved1;
    uint16_t tx_ready;
    uint32_t mode;
    uint32_t baud_rate;
    uint16_t in_proto_mask;
    uint16_t out_proto_mask;
    uint16_t flags;
    uint8_t reserved2[2];
} ubx_cfg_prt_t;


typedef struct __attribute__((__packed__)) ubx_cfg_rxm_s {
    uint8_t reserved;
    uint8_t lp_mode;
} ubx_cfg_rxm_t;

typedef struct __attribute__((__packed__)) ubx_cfg_rate_s {
    uint16_t meas_rate;
    uint16_t nav_rate;
    uint16_t time_ref;
} ubx_cfg_rate_t;

typedef union ubx_cfg_u {
    ubx_cfg_prt_t prt;
    ubx_cfg_rxm_t rxm;
    ubx_cfg_rate_t rate;
} ubx_cfg_t;

typedef union ubx_msg_u {
    ubx_mon_ver_t ver;
    ubx_nav_t nav;
    ubx_cfg_t cfg;
} ubx_msg_t;

static can5_sensor_hdl_t *alloc(size_t *len);

static uint8_t get_sensor_id(can5_sensor_hdl_t *hdl);

static void set_sensor_id(can5_sensor_hdl_t *hdl, uint8_t sensor_id);

static can5_err_t detect(can5_sensor_hdl_t *hdl, can5_port_idx_t port);

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb);

static can5_err_t uninit(can5_sensor_hdl_t *hdl);

static can5_err_t run(can5_sensor_hdl_t *hdl);

static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t read_end_timeout, bool blocking);

static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f *);

static can5_err_t enable(can5_sensor_hdl_t *hdl, bool enable);

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len);

static bool is_running(can5_sensor_hdl_t *hdl);

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response);

static int32_t status_get(can5_sensor_hdl_t *hdl);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status);

#endif


const can5_sensordriv_t sensordriv_ublox_neo = {
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
            .read_time = CONFIG_CAN5_UBLOX_M8M_READ_TIME,
            .type = CAN5_SENSORDRIV_TYPE_GPS,
        },
        .io_type = CAN5_PHY_IO_TYPE_UART,
        .name = "NEO",
        .version = "1.0",
        .manufacturer = "UBLOX",
    }
};

typedef struct gps_rx_buf_s {
    volatile bool start_found;
    char buf[UBX_MAX_LEN];
    size_t len;
    int64_t timestamp;
} gps_rx_buf_t;

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_ublox_neo_status_t status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_ublox_neo_data_t cb_data;
    can5_sensor_hwcb_f *hwcb;
    gps_rx_buf_t rx_buf;
    time_t read_time_end;
    ubx_nav_pvt_t last_good_fix;
    //uint16_t n_satellites_view;
    //uint16_t n_satellites_active;
} sensor_hdl_t;

#define SENSOR_HW_EVT(hdl, evt_type)                \
do {                                                \
    sensor_hdl_t *ss_hdl = (hdl)->hdl;\
    if (ss_hdl->hwcb) {                             \
        can5_sensor_hw_evt_t evt = {                \
            .type = (evt_type),                     \
            .sensor_id = get_sensor_id(hdl),        \
        };                                          \
        ss_hdl->hwcb(&evt);                         \
    }                                               \
} while (0)

typedef struct sensor_ubx_data_s {
    bool read;
    size_t len;
    char buf[UBX_MAX_LEN];
} sensor_ubx_data_t;

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
void __print_gps_data(const can5_sensor_ublox_neo_data_t *gps_data);

static void __flush_uart_rx(sensor_hdl_t *hdl);

static can5_err_t __read_gps(sensor_hdl_t *s_hdl);

static can5_err_t __cmd(sensor_hdl_t *s_hdl, const char *ubx_msg, size_t ubx_msg_len, sensor_ubx_data_t *data, time_t timeout);

static can5_err_t __comm(sensor_hdl_t *s_hdl,
                         int32_t msg_class, int32_t msg_id,
                         const char * payload, size_t payload_len,
                         int32_t *rx_msg_class, int32_t *rx_msg_id,
                         char *rx_payload, size_t *max_rx_payload);

static can5_err_t __prepare_ubx_channel(sensor_hdl_t *s_hdl, bool is_realtime);

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
    s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_UNINITD;

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
    sensor_hdl_t *s_hdl = hdl->hdl;
    int32_t msg_class, msg_id;

    if (s_hdl->port == port) {
        return CAN5_SUCCESS;
    }

    s_hdl->port = port;

    ubx_mon_ver_t ubx_version;
    CLEAR_STRUCT(ubx_version);
    size_t max_payload_size = sizeof(ubx_mon_ver_t);

    VERIFY_SUCCESS(__comm(s_hdl,
                          0x0a, 0x04, NULL, 0,
                          &msg_class, &msg_id,
                          (char *)&ubx_version, &max_payload_size));


    ESP_LOGI(TAG, "Detected UBLOX GPS module %s %s %s!", ubx_version.sw_version, ubx_version.hw_version, ubx_version.extenstion);

    return CAN5_SUCCESS;
}

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    char *data_mode;
    bool is_realtime = false;

    // verify that the module is present in the specified port
    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }

    data_mode = NULL;
    if(config_manager.read(CFG_DEVICE_DATA_MODE, (uint8_t **)&data_mode, NULL) == CAN5_SUCCESS) {
        is_realtime = strcmp(data_mode, can5_device_data_mode_get_str(CAN5_DATA_MODE_REALTIME)) == 0;
    }
    if (data_mode) {
        free(data_mode);
    }

    __prepare_ubx_channel(s_hdl, is_realtime);

    s_hdl->last_good_fix.fix_type = 0;

    s_hdl->hwcb = hwcb;
    s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_UNINITD;
    s_hdl->port = CAN5_PORT_NULL;

    return CAN5_SUCCESS;
}

static void __ubx_nav_pvt_to_gps_data(ubx_nav_pvt_t *ubx, can5_sensor_ublox_neo_data_t *gps_data)
{
    gps_data->lng = ubx->lon * 0.0000001;
    gps_data->lat = ubx->lat * 0.0000001;
    gps_data->alt = ubx->height_mean_sea_level * 0.001;
    gps_data->vel_north = ubx->vel_n * 0.001;
    gps_data->vel_east = ubx->vel_e * 0.001;
    gps_data->vel_down = ubx->vel_d * 0.001;
    gps_data->n_sat = ubx->num_sv;
    gps_data->fix = gps_data->read = true;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    can5_err_t ret;

    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_ublox_neo_data_t *gps_data;
        CLEAR_STRUCT(s_hdl->cb_data);
        gps_data = &s_hdl->cb_data;

        ret = __read_gps(s_hdl);

        if (ret == CAN5_SUCCESS) {
            __ubx_nav_pvt_to_gps_data(&s_hdl->last_good_fix, gps_data);

            s_hdl->readcb(get_sensor_id(hdl), gps_data, sizeof(*gps_data));
            SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);

            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_INITD;
        }
        else if (s_hdl->read_time_end <= can5_time_ms(NULL)) {
            if (s_hdl->last_good_fix.valid & PVT_FULLY_RESOLVED) {
                __ubx_nav_pvt_to_gps_data(&s_hdl->last_good_fix, gps_data);
                s_hdl->readcb(get_sensor_id(hdl), gps_data, sizeof(*gps_data));
                SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);
            }
            else {
                SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_FAILURE);
            }
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_INITD;
        }

    }

    return CAN5_SUCCESS;
}


/**
 * @brief Read sensor data
 * @param prxdata  buffer to read data into, can be null if blocking is false
 * @param plen  size to write to, can be null if blocking is false
 * @param blocking if this function should block or dispatch read
 */
static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t read_end_timeout, bool blocking)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    if (s_hdl->status == SENSORDRIV_UBLOX_NEO_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_READING;
    s_hdl->read_time_end = read_end_timeout;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_UBLOX_NEO_STAT_INITD;
        return CAN5_ERR_UNIMPLEMENTED;
    } else {
        // dispatch the read to run() and call read_cb when done
        s_hdl->async_read = true;
    }


    return CAN5_SUCCESS;
}

static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f *cb)
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
    const can5_sensor_ublox_neo_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (!s_data->fix) {
        goto done;
    }

    if (sizeof(can5_sensor_ublox_neo_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->lat);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }


    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->lng);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }


    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->alt);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_NORTH, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->vel_north);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }


    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_EAST, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->vel_east);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }


    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_DOWN, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, 0, 0, s_data->vel_down);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

done:
    return list;
}

static bool is_running(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return s_hdl->status >= SENSORDRIV_UBLOX_NEO_STAT_INITD;
}

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl;
    size_t len;

    s_hdl = NULL;
    len = 0;

    // requesting null handle because there is no handle
    if (request != CAN5_SENSOR_UBLOX_NEO_GPS_DRIVERCTL_GET_NULL_HDL) {
        s_hdl = hdl->hdl;
        CHECK_INITD(s_hdl);
    }

    switch (request) {
        case CAN5_SENSOR_UBLOX_NEO_GPS_DRIVERCTL_PRINT:
            __print_gps_data((can5_sensor_ublox_neo_data_t *) params);
            break;
        case CAN5_SENSOR_UBLOX_NEO_GPS_DRIVERCTL_GET_NULL_HDL:
            *(can5_sensor_hdl_t **)response =  alloc(&len);
            break;
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

void __print_gps_data(const can5_sensor_ublox_neo_data_t *gps_data)
{
    ESP_LOGI(TAG, "Fix: %d Lat: %f Lng: %f Alt: %f Active Satellites: %hhu",
             gps_data->fix, gps_data->lat, gps_data->lng, gps_data->alt, gps_data->n_sat);
}

static void __flush_uart_rx(sensor_hdl_t *s_hdl)
{
    TRACE_FUNC;

    char buf;
    size_t len = 1;
    while (hal.serial_recv(&buf, &len, s_hdl->port, 0) == CAN5_SUCCESS);
    s_hdl->rx_buf.start_found = false;
    s_hdl->rx_buf.len = 0;
    CLEAR_ARRAY(s_hdl->rx_buf.buf);
}

static can5_err_t __read_gps(sensor_hdl_t *s_hdl)
{
    TRACE_FUNC;
    int32_t msg_class, msg_id;
    ubx_nav_pvt_t ubx_nav_pvt;
    size_t ubx_nav_pvt_len = sizeof (ubx_nav_pvt_t);

    if (__comm(s_hdl,
           0x01, 0x07, NULL, 0,
           &msg_class, &msg_id,
               (char *) &ubx_nav_pvt, &ubx_nav_pvt_len
           ) == CAN5_SUCCESS) {


#if CONFIG_CAN5_SENSOR_GPS_TIME_SYNC
#pragma message("Time Sync with GPS enabled.")
            if (ubx_nav_pvt.valid & (PVT_VALID_DATE | PVT_VALID_TIME)) {
                struct tm tm = {
                    .tm_year = ubx_nav_pvt.year - 1900,
                    .tm_mon = ubx_nav_pvt.month - 1,
                    .tm_mday = ubx_nav_pvt.day,
                    .tm_hour = ubx_nav_pvt.hour,
                    .tm_min = ubx_nav_pvt.min,
                    .tm_sec = ubx_nav_pvt.sec,
                    .tm_isdst = 0,
                };

                VERIFY_SUCCESS(rtc.adjust_gps_time(&tm, s_hdl->rx_buf.timestamp));
            }
#endif

        // gnssFixOk
        if (ubx_nav_pvt.valid & PVT_FULLY_RESOLVED) {
            s_hdl->last_good_fix = ubx_nav_pvt;

            return CAN5_SUCCESS;
        }
    }

    return CAN5_SENSOR_ERR_BUSY;
}

static can5_err_t __parse_read_rx(sensor_hdl_t *s_hdl, char buf, char *out_buf, size_t *out_len)
{
    bool read_err = false;
    uint16_t len;

    gps_rx_buf_t *rx_buf;
    rx_buf = &s_hdl->rx_buf;
    len = 0;

    if (rx_buf->len + 1 > UBX_MAX_LEN) {
        return CAN5_SENSOR_ERR_PARSE_ERROR;
    }

    if (rx_buf->len > 6) {
        len = *((uint16_t *)&rx_buf->buf[4]);
    }

    if (rx_buf->start_found && rx_buf->len > 0 &&
        buf == UBLOX_NEO_START_BYTE2 && rx_buf->buf[rx_buf->len - 1] == UBLOX_NEO_START_BYTE1) {
        rx_buf->buf[0] = UBLOX_NEO_START_BYTE1;
        rx_buf->len = 1;
    }

    if (!rx_buf->start_found && buf == UBLOX_NEO_START_BYTE1) {
        rx_buf->start_found = true;
        rx_buf->timestamp = esp_timer_get_time();
        rx_buf->len = 0;
    }
    else if (rx_buf->start_found &&
             rx_buf->len == (U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES + len - 1)) {
        rx_buf->buf[rx_buf->len++] = buf;  // store checksum
        rx_buf->start_found = false;
        *out_len = rx_buf->len;
        memcpy(out_buf, rx_buf->buf, rx_buf->len);
        rx_buf->len = 0;
        return CAN5_SUCCESS;
    }

    if (rx_buf->start_found) {
        rx_buf->buf[rx_buf->len++] = buf;
    }

    if (rx_buf->len == 1) {
        read_err = buf != UBLOX_NEO_START_BYTE2;
    }

    if (read_err) {
        return CAN5_SENSOR_ERR_PARSE_ERROR;
    }

    return CAN5_SENSOR_ERR_PARSE_INCOMPLETE;
}


static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, sensor_ubx_data_t *data, time_t timeout)
{
    TRACE_FUNC;
    char buf;
    size_t len;

    time_t start = can5_time_ms(NULL);
    while (can5_time_ms(NULL) <= start + timeout) {
        len = 1;
        hal.serial_recv(&buf, &len, s_hdl->port, SERIAL_TIMEOUT);
        if (len == 1) {
            CLEAR_STRUCT(*data);
            if (__parse_read_rx(s_hdl, buf, data->buf, &data->len) == CAN5_SUCCESS) {
                data->read = true;
                ESP_LOG_BUFFER_HEXDUMP_V(TAG, data->buf, data->len, ESP_LOG_INFO);
                return CAN5_SUCCESS;
            }

        }
        taskYIELD();
    }

    return CAN5_SENSOR_ERR_PARSE_INCOMPLETE;
}

static can5_err_t __cmd(sensor_hdl_t *s_hdl, const char *ubx_msg, size_t ubx_msg_len, sensor_ubx_data_t *data, time_t timeout)
{
    TRACE_FUNC;

    __flush_uart_rx(s_hdl);

    VERIFY_SUCCESS(hal.serial_send(ubx_msg, ubx_msg_len, s_hdl->port, SERIAL_TIMEOUT));

    VERIFY_SUCCESS(__sensor_read_rx(s_hdl, data, timeout));

    return CAN5_SUCCESS;
}

static can5_err_t __comm(sensor_hdl_t *s_hdl,
                         int32_t msg_class, int32_t msg_id,
                         const char * payload, size_t payload_len,
                         int32_t *rx_msg_class, int32_t *rx_msg_id,
                         char *rx_payload, size_t *max_rx_payload)
{

    int32_t buffer_len;
    char buffer[U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES + sizeof(ubx_msg_t) + 2];

    if ((buffer_len = uUbxProtocolEncode(msg_class, msg_id, (const char *) payload, payload_len, buffer)) > 0) {
        sensor_ubx_data_t data;
        CLEAR_STRUCT(data);

        buffer[buffer_len] = '\r';
        buffer[buffer_len + 1] = '\n';

        if (__cmd(s_hdl, buffer, buffer_len, &data, SERIAL_TIMEOUT) == CAN5_SUCCESS) {
            if (data.read) {
                if ((buffer_len = uUbxProtocolDecode(data.buf, data.len, rx_msg_class, rx_msg_id, rx_payload,
                                                     *max_rx_payload, NULL)) > 0) {
                    *max_rx_payload = buffer_len;
                    return CAN5_SUCCESS;
                }
            }
        }
    }

    ESP_LOGE_V(TAG, "cannot decode ubx message");
    return CAN5_SENSOR_ERR_PARSE_ERROR;
}

static can5_err_t __prepare_ubx_channel(sensor_hdl_t *s_hdl, bool is_realtime)
{
    TRACE_FUNC;
    int32_t msg_class, msg_id;
    size_t max_payload_bytes;

    /*
     * b5 62 06 00 |14 00| 01| 00 |  00 00 | c0 08 00 00 |80 25
     * 00 00 | 07 00 | 03 00 | 00 00  |00 00 92 b5
     */

    ubx_cfg_prt_t ubx_cfg_prt = {
        .port_id = 0x01,
        .tx_ready = 0,
        .mode = 0x8C0,
        .baud_rate = 9600,
        .in_proto_mask = 0x01,
        .out_proto_mask = 0x01,
        .flags = 0,
    };

    max_payload_bytes = 0;
    CAN5_ERR_CHECK_NO_ABORT(__comm(s_hdl, 0x06, 0x00, (const char *) &ubx_cfg_prt, sizeof(ubx_cfg_prt_t),
           &msg_class, &msg_id, NULL, &max_payload_bytes));

    ubx_cfg_rxm_t ubx_cfg_rxm = {
        .reserved = 0,
        .lp_mode = 1, // power save mode
    };

    if (is_realtime) {
        ubx_cfg_rxm.lp_mode = 0; // continuous
    }

    max_payload_bytes = 0;
    CAN5_ERR_CHECK_NO_ABORT(__comm(s_hdl, 0x06, 0x11, (const char *) &ubx_cfg_rxm, sizeof(ubx_cfg_rxm_t),
                                   &msg_class, &msg_id, NULL, &max_payload_bytes));

    ubx_cfg_rate_t ubx_cfg_rate = {
        .meas_rate = can5_sec_to_ms(CONFIG_CAN5_UBLOX_M8M_READ_TIME)/2,
        .nav_rate = 1,
        .time_ref = 0,
    };

    if (is_realtime) {
        ubx_cfg_rate.meas_rate = 250; // 4 Hz, 250 ms
    }

    max_payload_bytes = 0;
    CAN5_ERR_CHECK_NO_ABORT(__comm(s_hdl, 0x06, 0x08, (const char *) &ubx_cfg_rate, sizeof(ubx_cfg_rate_t),
                                   &msg_class, &msg_id, NULL, &max_payload_bytes));

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
static const can5_tag_tab_t _SENSORDRIV_UBLOX_NEO_STAT_tags = {
    TAG_TAB_ITEM(SENSORDRIV_UBLOX_NEO_STAT_UNINITD),
    TAG_TAB_ITEM(SENSORDRIV_UBLOX_NEO_STAT_INITD),
    TAG_TAB_ITEM(SENSORDRIV_UBLOX_NEO_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_UBLOX_NEO_STAT_tags);
}

#endif

/* ---------------------------------------------------------------------
 * Plugin Parser
 -----------------------------------------------------------------------*/

void parse_gps_read_data(const can5_sensordriv_t *driv, const can5_sensor_ublox_neo_data_t *gps_data, void *pout, size_t *plen)
{
    if (!gps_data->fix) return;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%lf,%s:%lf,%s:%lf,%s:%lf,%s:%lf,%s:%lf,%s:%hhu",
             can5_hazemon_get_type(HAZEMON_GPS_LAT)->token, gps_data->lat,
             can5_hazemon_get_type(HAZEMON_GPS_LNG)->token, gps_data->lng,
             can5_hazemon_get_type(HAZEMON_GPS_ALT)->token, gps_data->alt,
             "Velocity North", gps_data->vel_north,
             "Velocity East", gps_data->vel_east,
             "Velocity Down", gps_data->vel_down,
             "Satellites", gps_data->n_sat
    );
    *plen = strlen(pout);
}
