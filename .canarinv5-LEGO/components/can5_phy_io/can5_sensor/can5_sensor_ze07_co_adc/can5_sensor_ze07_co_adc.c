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
#include "can5_sensor_ze07_co_adc.h"
#include "can5_hazemon_types.h"
#include "can5_config.h"

#define TAG "SENSOR_CO"

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_CO_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI_V(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif


#define SENSOR_READ_TIME            1   //2 seconds

typedef enum sensordriv_co_status_e {
    SENSORDRIV_CO_STAT_UNINITD = 0,
    SENSORDRIV_CO_STAT_INITD,
    SENSORDRIV_CO_STAT_READING,
} sensordriv_co_status_t;

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


const can5_sensordriv_t sensordriv_ze07_co_adc = {
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
            .read_time = 60,
            .type = CAN5_SENSORDRIV_TYPE_CO_ADC,
        },
        .io_type = CAN5_PHY_IO_TYPE_ADC,
        .name = "ZE07-CO",
        .version = "1.0",
        .manufacturer = "Winsen",
    }
};

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_co_status_t  status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_ze07_co_adc_t cb_data;
    can5_sensor_hwcb_f *hwcb;
    struct {
        double multiplier;
        double bias;
    } calib;
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
static double __get_reading(sensor_hdl_t *s_hdl);

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
    s_hdl->status = SENSORDRIV_CO_STAT_UNINITD;

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
    uint16_t sample;

    hal.analog_read(&sample, port);

    ESP_LOGI_V(TAG, "read data %d", sample);
    return CAN5_SUCCESS;
}

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    s_hdl->hwcb = hwcb;
    s_hdl->port = port;

    VERIFY_SUCCESS(config_manager.read_double(CFG_CALIB_ZE07_CO_ADC_MULTI, &s_hdl->calib.multiplier));
    VERIFY_SUCCESS(config_manager.read_double(CFG_CALIB_ZE07_CO_ADC_BIAS, &s_hdl->calib.bias));

    ESP_LOGI(TAG, "Calibration: %f * adc + %f", s_hdl->calib.multiplier ,s_hdl->calib.bias);

    s_hdl->status = SENSORDRIV_CO_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_CO_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        // read here
        can5_ze07_co_adc_t *co_data;
        CLEAR_STRUCT(s_hdl->cb_data);
        co_data = &s_hdl->cb_data;

        s_hdl->async_read = false;

        co_data->val = __get_reading(s_hdl);

        s_hdl->status = SENSORDRIV_CO_STAT_INITD;
        // callback read data here
        s_hdl->readcb(get_sensor_id(hdl), co_data,  sizeof (*co_data));
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
    if (s_hdl->status == SENSORDRIV_CO_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_CO_STAT_READING;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_CO_STAT_INITD;
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
    const can5_ze07_co_adc_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (sizeof(can5_ze07_co_adc_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_ZE07_CO, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->val);
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

    return s_hdl->status >= SENSORDRIV_CO_STAT_INITD;
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

static inline double calibration(sensor_hdl_t  *hdl, uint16_t val)
{
    double result;
    result = (hdl->calib.multiplier * val) + hdl->calib.bias;

    //ESP_LOGI(TAG,"%f * %d  + %f = %f", hdl->calib.multiplier, val, hdl->calib.bias, result);
    return result;
}

static double __get_reading(sensor_hdl_t *s_hdl)
{
    uint16_t val;
    double calib_avg;
    size_t n;
    time_t end = can5_time_ms(NULL) + can5_sec_to_ms(SENSOR_READ_TIME);

    n = 0;
    calib_avg = 0;
    while (can5_time_ms(NULL) <= end) {
        double calib;
        if (hal.analog_read(&val, s_hdl->port) != CAN5_SUCCESS) {
            continue;
        }
        calib = calibration(s_hdl, val);
        if (calib < 0) {
            continue;
        }

        calib_avg += (calib - calib_avg) / (n + 1);

        // ESP_LOGI(TAG, "CO val %f avg: %f", calib, calib_avg);

        n++;
    }

    return calib_avg;
}
//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief NEtif Status tags
 *
 */
static const can5_tag_tab_t _SENSORDRIV_CO_STAT_tags = {
        TAG_TAB_ITEM(SENSORDRIV_CO_STAT_UNINITD),
        TAG_TAB_ITEM(SENSORDRIV_CO_STAT_INITD),
        TAG_TAB_ITEM(SENSORDRIV_CO_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_CO_STAT_tags);
}

#endif
void parse_co_adc_read_data(const can5_sensordriv_t *driv, const can5_ze07_co_adc_t *co_data, void *pout, size_t *plen)
{
    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%.4f",
             can5_hazemon_get_type(HAZEMON_MQ7_CO)->token, co_data->val);
    *plen = strlen(pout);
}
