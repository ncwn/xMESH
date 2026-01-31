/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   24/01/22
*
* File:  can5_sensor_co2_mg_z16.c
* Descr:
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_sensor_ze07_co.h"
#include "can5_hazemon_types.h"
#include "can5_config.h"


static const char *TAG = "SENSOR_ZE07-CO";

#define ZE07CO_START_BYTE          0xFF
#define ZE07CO_READ_CMD            0x86
#define ZE07CO_COMM_MODE           0x78
#define ZE07CO_CMD_POS             1
#define ZE07CO_READ_HIGH_POS       2
#define ZE07CO_READ_LOW_POS        3
#define ZE07CO_READ_RETURN_CALIB   2
#define ZE07CO_MAX_LENGTH          9

static uint8_t READ_CO_CMD[] = {0XFF, 0x01, ZE07CO_READ_CMD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
static uint8_t MODIFY_COMM_MODE_CMD[] = {0XFF, 0x01, ZE07CO_COMM_MODE, 0x41, 0x00, 0x00, 0x00, 0x00, 0x46};

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_ZE07CO_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#define SERIAL_TIMEOUT              500

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_ze07co_status_e {
    SENSORDRIV_ZE07CO_STAT_UNINITD = 0,
    SENSORDRIV_ZE07CO_STAT_INITD,
    SENSORDRIV_ZE07CO_STAT_STAT_READING,
} sensordriv_ze07co_status_t;


static can5_sensor_hdl_t *alloc(size_t *len);

static uint8_t get_sensor_id(can5_sensor_hdl_t *hdl);

static void set_sensor_id(can5_sensor_hdl_t *hdl, uint8_t sensor_id);

static can5_err_t detect(can5_sensor_hdl_t *hdl, can5_port_idx_t port);

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb);

static can5_err_t uninit(can5_sensor_hdl_t *hdl);

static can5_err_t run(can5_sensor_hdl_t *hdl);

static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t read_time_end, bool blocking);

static can5_err_t register_read_cb(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f *);

static can5_err_t enable(can5_sensor_hdl_t *hdl, bool enable);

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len);

static bool is_running(can5_sensor_hdl_t *hdl);

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response);

static int32_t status_get(can5_sensor_hdl_t *hdl);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status);

#endif

const can5_sensordriv_t sensordriv_ze07_co = {
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
            .type = CAN5_SENSORDRIV_TYPE_CO,
        },
        .io_type = CAN5_PHY_IO_TYPE_UART,
        .name = "ZE07-CO",
        .version = "1.0",
        .manufacturer = "Winsen",
    }
};

typedef struct co_rx_buf_s {
    volatile bool start_found;
    char buf[ZE07CO_MAX_LENGTH + 1];
    size_t len;
} co_rx_buf_t;

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_ze07co_status_t status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_ze07_co_data_t cb_data;
    can5_sensor_hwcb_f *hwcb;
    co_rx_buf_t rx_buf;
    double bias;
    time_t read_time_end;
    struct {
        can5_sensor_ze07_co_data_t data;
        ssize_t n;
    } read;
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

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t __parse_read_rx(sensor_hdl_t *s_hdl, char buf, char *out_buf);

static void __flush_uart_rx(sensor_hdl_t *s_hdl);

static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, can5_sensor_ze07_co_data_t *co_data, time_t timeout);

static can5_err_t __change_to_qa_mode(sensor_hdl_t *s_hdl);

static can5_err_t __read_co(sensor_hdl_t *s_hdl, can5_sensor_ze07_co_data_t *co_data);

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
    s_hdl->status = SENSORDRIV_ZE07CO_STAT_UNINITD;

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
    if (s_hdl->port == port) {
        return CAN5_SUCCESS;
    }

    can5_sensor_ze07_co_data_t co_data;

    CLEAR_STRUCT(co_data);
    s_hdl->port = port;

    __flush_uart_rx(s_hdl);
    VERIFY_SUCCESS(__change_to_qa_mode(s_hdl));
    if (__read_co(s_hdl, &co_data) != CAN5_SUCCESS) {
        s_hdl->port = CAN5_PORT_NULL;
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    ESP_LOGI(TAG, "Detected ZE07-CO module!");

    return CAN5_SUCCESS;
}

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }

    VERIFY_SUCCESS(config_manager.read_double(CFG_CALIB_ZE07_CO_BIAS, &s_hdl->bias));

    ESP_LOGI(TAG, "Calibration bias: %f", s_hdl->bias);

    s_hdl->hwcb = hwcb;
    s_hdl->status = SENSORDRIV_ZE07CO_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_ZE07CO_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    // TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    can5_err_t ret;
    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_ze07_co_data_t co_data_inst;
        can5_sensor_ze07_co_data_t *co_data;
        CLEAR_STRUCT(co_data_inst);

        ret = __read_co(s_hdl, &co_data_inst);

        if (ret == CAN5_SUCCESS && co_data_inst.read) {
            s_hdl->read.n++;
            s_hdl->read.data.val += (co_data_inst.val - s_hdl->read.data.val) / s_hdl->read.n;
        }

        if (s_hdl->read_time_end <= can5_time_ms(NULL)) {
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_ZE07CO_STAT_INITD;

            if (s_hdl->read.n > 0) {
                s_hdl->read.data.read = true;

                CLEAR_STRUCT(s_hdl->cb_data);
                co_data = &s_hdl->cb_data;
                *co_data = s_hdl->read.data;
                ESP_LOGI(TAG, "Averaged %d datapoints", s_hdl->read.n);

                s_hdl->readcb(get_sensor_id(hdl), co_data, sizeof(*co_data));
                SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_COMPLETE);
            }
            else {
                SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_READ_FAILURE);
            }
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
static can5_err_t read(can5_sensor_hdl_t *hdl, void *prxdata, size_t *plen, time_t read_time_end, bool blocking)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);
    if (s_hdl->status == SENSORDRIV_ZE07CO_STAT_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_ZE07CO_STAT_STAT_READING;

    CLEAR_STRUCT(s_hdl->read.data);
    s_hdl->read.n = 0;

    s_hdl->read_time_end = read_time_end;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_ZE07CO_STAT_INITD;
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
    const can5_sensor_ze07_co_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (!s_data->read) {
        goto done;
    }

    if (sizeof(can5_sensor_ze07_co_data_t) != data_len) {
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

    return s_hdl->status >= SENSORDRIV_ZE07CO_STAT_INITD;
}

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response)
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

static can5_err_t __parse_read_rx(sensor_hdl_t *s_hdl, char buf, char *out_buf)
{
    // TRACE_FUNC_; /* Too frequent */
    bool read_err = false;

    co_rx_buf_t *rx_buf;
    rx_buf = &s_hdl->rx_buf;

    if (rx_buf->len + 1 > ZE07CO_MAX_LENGTH + 1) {
        return CAN5_SENSOR_ERR_PARSE_ERROR;
    }

    if (buf == ZE07CO_START_BYTE) {
        /* Check start byte */
        rx_buf->start_found = true;
        rx_buf->len = 0;
    } else if (rx_buf->start_found && rx_buf->len == (ZE07CO_MAX_LENGTH - 1)) {
        /* Check end condition, i.e. length */
        rx_buf->buf[rx_buf->len++] = buf;  // store checksum
        rx_buf->start_found = false;
        memcpy(out_buf, rx_buf->buf, rx_buf->len);
        rx_buf->len = 0;
        return CAN5_SUCCESS;
    }

    /* add char to buffer */
    if (rx_buf->start_found) {
        rx_buf->buf[rx_buf->len++] = buf;
    }

    if (rx_buf->len == ZE07CO_CMD_POS) {
        read_err = buf != ZE07CO_READ_CMD;
    }


    /* Check if max length is not crossed */
    read_err |= rx_buf->len >= ZE07CO_MAX_LENGTH;

    if (read_err) {
        return CAN5_SENSOR_ERR_PARSE_ERROR;
    }
    return CAN5_SENSOR_ERR_PARSE_INCOMPLETE;
}

static void __flush_uart_rx(sensor_hdl_t *s_hdl)
{
    char buf;
    size_t len = 1;
    while (hal.serial_recv(&buf, &len, s_hdl->port, 0) == CAN5_SUCCESS);
    s_hdl->rx_buf.start_found = false;
    s_hdl->rx_buf.len = 0;
    CLEAR_ARRAY(s_hdl->rx_buf.buf);
}

static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, can5_sensor_ze07_co_data_t *co_data, time_t timeout)
{
    TRACE_FUNC;
    int64_t start;
    char buf;
    size_t len;
    char sentence[ZE07CO_MAX_LENGTH];

    CLEAR_ARRAY(sentence);

    start = can5_time_ms();

    while (can5_time_ms(NULL) <= start + timeout) {
        len = 1;
        hal.serial_recv(&buf, &len, s_hdl->port, SERIAL_TIMEOUT);
        if (len == 1) {
            if (__parse_read_rx(s_hdl, buf, sentence) == CAN5_SUCCESS) {
                co_data->read = true;
                co_data->val = (sentence[ZE07CO_READ_HIGH_POS]  * 256 + sentence[ZE07CO_READ_LOW_POS]) * 0.1;

                ESP_LOG_BUFFER_HEXDUMP_V(TAG, sentence, ZE07CO_MAX_LENGTH,  ESP_LOG_INFO);
                return CAN5_SUCCESS;
            }
        }
        taskYIELD();
    }

    return CAN5_SENSOR_ERR_PARSE_INCOMPLETE;
}

static can5_err_t __change_to_qa_mode(sensor_hdl_t *s_hdl)
{
    __flush_uart_rx(s_hdl);
    VERIFY_SUCCESS(hal.serial_send(MODIFY_COMM_MODE_CMD, sizeof(MODIFY_COMM_MODE_CMD), s_hdl->port, SERIAL_TIMEOUT));
    vTaskDelay(pdMS_TO_TICKS(10));
    return CAN5_SUCCESS;
}

static can5_err_t
__read_co(sensor_hdl_t *s_hdl, can5_sensor_ze07_co_data_t *co_data)
{
    TRACE_FUNC;

    __flush_uart_rx(s_hdl);

    VERIFY_SUCCESS(hal.serial_send(READ_CO_CMD, sizeof(READ_CO_CMD), s_hdl->port, SERIAL_TIMEOUT));

    VERIFY_SUCCESS(__sensor_read_rx(s_hdl, co_data, SERIAL_TIMEOUT));

    // update the bias
    co_data->val += s_hdl->bias;
    // limit to 0
    if(co_data->val < 0.0) {
        co_data->val = 0.0;
    }

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
static const can5_tag_tab_t _SENSORDRIV_ZE07CO_STAT_STAT_tags = {
    TAG_TAB_ITEM(SENSORDRIV_ZE07CO_STAT_UNINITD),
    TAG_TAB_ITEM(SENSORDRIV_ZE07CO_STAT_INITD),
    TAG_TAB_ITEM(SENSORDRIV_ZE07CO_STAT_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_ZE07CO_STAT_STAT_tags);
}

#endif

/* ---------------------------------------------------------------------
 * Plugin Parser
 -----------------------------------------------------------------------*/

void parse_co_read_data(const can5_sensordriv_t *driv, const can5_sensor_ze07_co_data_t *co_data, void *pout, size_t *plen)
{
    if (!co_data->read) return;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%.4f",
             can5_hazemon_get_type(HAZEMON_CO)->token, co_data->val);
    *plen = strlen(pout);
}
