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
#include "can5_sensor_ze03_no2.h"
#include "can5_hazemon_types.h"


static const char *TAG = "SENSOR_ZE03-NO2";

#define ZE03NO2_START_BYTE          0xFF
#define ZE03NO2_READ_CMD            0x86
#define ZE03NO2_COMM_MODE           0x78
#define ZE03NO2_CMD_POS             1
#define ZE03NO2_READ_HIGH_POS       2
#define ZE03NO2_READ_LOW_POS        3
#define ZE03NO2_READ_RETURN_CALIB   2
#define ZE03NO2_MAX_LENGTH          9

static uint8_t READ_NO2_CMD[] = {0XFF, 0x01, ZE03NO2_READ_CMD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
static uint8_t MODIFY_COMM_MODE_CMD[] = {0XFF, 0x01, ZE03NO2_COMM_MODE, 0x04, 0x00, 0x00, 0x00, 0x00, 0x83};

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_ZE03NO2_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#define SERIAL_TIMEOUT              500

#if 0
#define TRACE_FUNC ESP_LOGI_V(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_ze03no2_status_e {
    SENSORDRIV_ZE03NO2_STAT_UNINITD = 0,
    SENSORDRIV_ZE03NO2_STAT_INITD,
    SENSORDRIV_ZE03NO2_STAT_STAT_READING,
} sensordriv_ze03no2_status_t;


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

const can5_sensordriv_t sensordriv_ze03_no2 = {
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
            .type = CAN5_SENSORDRIV_TYPE_NO2,
        },
        .io_type = CAN5_PHY_IO_TYPE_UART,
        .name = "ZE03-NO2",
        .version = "1.0",
        .manufacturer = "Winsen",
    }
};

typedef struct no2_rx_buf_s {
    volatile bool start_found;
    char buf[ZE03NO2_MAX_LENGTH + 1];
    size_t len;
} no2_rx_buf_t;

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_ze03no2_status_t status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_ze03_no2_data_t cb_data;
    can5_sensor_hwcb_f *hwcb;
    no2_rx_buf_t rx_buf;
    time_t read_time_end;
    struct {
        can5_sensor_ze03_no2_data_t data;
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

typedef enum cmd_type_e {
    MODIFY_COM_MODE,
    READ_CONC_VAL,
} cmd_type_t;

static can5_err_t __parse_read_rx(sensor_hdl_t *s_hdl, char buf, char *out_buf, cmd_type_t cmd_type);

static void __flush_uart_rx(sensor_hdl_t *s_hdl);

static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, can5_sensor_ze03_no2_data_t *no2_data, cmd_type_t cmd_type, time_t timeout);

static can5_err_t __read_no2(sensor_hdl_t *s_hdl, can5_sensor_ze03_no2_data_t *no2_data, cmd_type_t cmd_type);

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
    s_hdl->status = SENSORDRIV_ZE03NO2_STAT_UNINITD;

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

    can5_sensor_ze03_no2_data_t no2_data;

    CLEAR_STRUCT(no2_data);
    s_hdl->port = port;

    if (__read_no2(s_hdl, &no2_data, MODIFY_COM_MODE) != CAN5_SUCCESS) {
        s_hdl->port = CAN5_PORT_NULL;
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    ESP_LOGI(TAG, "Detected ZE03-NO2 module!");

    return CAN5_SUCCESS;
}

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }

    s_hdl->hwcb = hwcb;
    s_hdl->status = SENSORDRIV_ZE03NO2_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_ZE03NO2_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    // TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    can5_err_t ret;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_ze03_no2_data_t no2_data_inst;
        can5_sensor_ze03_no2_data_t *no2_data;
        CLEAR_STRUCT(no2_data_inst);

        ret = __read_no2(s_hdl, &no2_data_inst, READ_CONC_VAL);

        if (ret == CAN5_SUCCESS && no2_data_inst.read) {
            s_hdl->read.n++;
            s_hdl->read.data.val += (no2_data_inst.val - s_hdl->read.data.val) / s_hdl->read.n;
        }

        if (s_hdl->read_time_end < can5_time_ms(NULL)) {
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_ZE03NO2_STAT_INITD;

            if (s_hdl->read.n >= 0) {
                s_hdl->read.data.read = true;

                CLEAR_STRUCT(s_hdl->cb_data);
                no2_data = &s_hdl->cb_data;
                *no2_data = s_hdl->read.data;
                ESP_LOGI(TAG, "Averaged %d datapoints", s_hdl->read.n);

                s_hdl->readcb(get_sensor_id(hdl), no2_data, sizeof(*no2_data));
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
    if (s_hdl->status == SENSORDRIV_ZE03NO2_STAT_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_ZE03NO2_STAT_STAT_READING;

    CLEAR_STRUCT(s_hdl->read.data);
    s_hdl->read.n = 0;

    s_hdl->read_time_end = read_time_end;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_ZE03NO2_STAT_INITD;
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
    const can5_sensor_ze03_no2_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (!s_data->read) {
        goto done;
    }

    if (sizeof(can5_sensor_ze03_no2_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_ZE03_NO2, s_hdl->port,
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

    return s_hdl->status >= SENSORDRIV_ZE03NO2_STAT_INITD;
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

static can5_err_t __parse_read_rx(sensor_hdl_t *s_hdl, char buf, char *out_buf, cmd_type_t cmd_type)
{
    // TRACE_FUNC_; /* Too frequent */
    bool read_err = false;

    no2_rx_buf_t *rx_buf;
    rx_buf = &s_hdl->rx_buf;

    if (rx_buf->len + 1 > ZE03NO2_MAX_LENGTH + 1) {
        return CAN5_SENSOR_ERR_PARSE_ERROR;
    }

    if (buf == ZE03NO2_START_BYTE) {
        /* Check start byte */
        rx_buf->start_found = true;
        rx_buf->len = 0;
    } else if (rx_buf->start_found && rx_buf->len == (ZE03NO2_MAX_LENGTH - 1)) {
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

    if (rx_buf->len == ZE03NO2_CMD_POS) {
        switch (cmd_type) {

            case MODIFY_COM_MODE:
                read_err = buf != ZE03NO2_COMM_MODE;
                break;
            case READ_CONC_VAL:
                /* Check if read command is correct */
                read_err = buf != ZE03NO2_READ_CMD;
                break;
        }

    }


    /* Check if max length is not crossed */
    read_err |= rx_buf->len >= ZE03NO2_MAX_LENGTH;

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

static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, can5_sensor_ze03_no2_data_t *no2_data, cmd_type_t cmd_type, time_t timeout)
{
    TRACE_FUNC;
    char buf;
    size_t len;
    char sentence[ZE03NO2_MAX_LENGTH];

    CLEAR_ARRAY(sentence);

    time_t start = can5_time_ms(NULL);
    while (can5_time_ms(NULL) <= start + timeout) {
        len = 1;
        hal.serial_recv(&buf, &len, s_hdl->port, SERIAL_TIMEOUT);
        if (len == 1) {
            if (__parse_read_rx(s_hdl, buf, sentence, cmd_type) == CAN5_SUCCESS) {
                no2_data->read = true;

                switch (cmd_type) {

                    case MODIFY_COM_MODE:
                        no2_data->val = sentence[ZE03NO2_READ_RETURN_CALIB];
                        break;
                    case READ_CONC_VAL:
                        no2_data->val = sentence[ZE03NO2_READ_HIGH_POS]  * 256 + sentence[ZE03NO2_READ_LOW_POS];
                        break;
                }

                ESP_LOG_BUFFER_HEXDUMP_V(TAG, sentence, ZE03NO2_MAX_LENGTH,  ESP_LOG_INFO);
                return CAN5_SUCCESS;
            }
        }
        taskYIELD();
    }

    return CAN5_SENSOR_ERR_PARSE_INCOMPLETE;
}


static can5_err_t
__read_no2(sensor_hdl_t *s_hdl, can5_sensor_ze03_no2_data_t *no2_data, cmd_type_t cmd_type)
{
    TRACE_FUNC;

    __flush_uart_rx(s_hdl);

    switch (cmd_type) {

        case MODIFY_COM_MODE:
            VERIFY_SUCCESS(hal.serial_send(MODIFY_COMM_MODE_CMD, sizeof(MODIFY_COMM_MODE_CMD), s_hdl->port, SERIAL_TIMEOUT));
            break;

        case READ_CONC_VAL:
            VERIFY_SUCCESS(hal.serial_send(READ_NO2_CMD, sizeof(READ_NO2_CMD), s_hdl->port, SERIAL_TIMEOUT));
            break;

        default:
            return CAN5_ERR_INVALID_PARAM;

    }

    VERIFY_SUCCESS(__sensor_read_rx(s_hdl, no2_data, cmd_type, SERIAL_TIMEOUT));

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
static const can5_tag_tab_t _SENSORDRIV_ZE03NO2_STAT_STAT_tags = {
    TAG_TAB_ITEM(SENSORDRIV_ZE03NO2_STAT_UNINITD),
    TAG_TAB_ITEM(SENSORDRIV_ZE03NO2_STAT_INITD),
    TAG_TAB_ITEM(SENSORDRIV_ZE03NO2_STAT_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_ZE03NO2_STAT_STAT_tags);
}

#endif

/* ---------------------------------------------------------------------
 * Plugin Parser
 -----------------------------------------------------------------------*/

void parse_no2_read_data(const can5_sensordriv_t *driv, const can5_sensor_ze03_no2_data_t *no2_data, void *pout, size_t *plen)
{
    if (!no2_data->read) return;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%u",
             can5_hazemon_get_type(HAZEMON_NO2)->token, no2_data->val);
    *plen = strlen(pout);
}
