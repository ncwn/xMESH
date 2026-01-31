/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   24/01/22
*
* File:  can5_sensor_pm_mg_z16.c
* Descr:
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_sensor_pms7003.h"
#include "can5_hazemon_types.h"


static const char *TAG = "SENSOR_PMS7003";

#define PMS7003_START_BYTE1         0x42    /* pos 1 */
#define PMS7003_START_BYTE2         0x4d    /* pos 2 */

#define PMS7003_CHECKSUM_LOW_POS    31
#define PMS7003_CHECKSUM_HIGH_POS   30

#define PMS7003_PM1_CF1_LOW_POS     5
#define PMS7003_PM1_CF1_HIGH_POS    4

#define PMS7003_PM2_5_CF1_LOW_POS   7
#define PMS7003_PM2_5_CF1_HIGH_POS  6

#define PMS7003_PM10_CF1_LOW_POS    9
#define PMS7003_PM10_CF1_HIGH_POS   8

#define PMS7003_PM1_ATE_LOW_POS     11
#define PMS7003_PM1_ATE_HIGH_POS    10

#define PMS7003_PM2_5_ATE_LOW_POS   13
#define PMS7003_PM2_5_ATE_HIGH_POS  12

#define PMS7003_PM10_ATE_LOW_POS    15
#define PMS7003_PM10_ATE_HIGH_POS   14

#define PMS7003_MAX_LENGTH          32

static uint8_t READ_PM_CMD[] = {0x42, 0x4d, 0xe2, 0x00, 0x00,0x01,  0x71};
static uint8_t MODIFY_COMM_MODE_CMD[] = {0x42, 0x4d, 0xe1, 0x00, 0x00, 0x01, 0x70};
static uint8_t MODIFY_COMM_MODE_ACK[] = { 0x42, 0x4d, 0x0, 0x04, 0xe1, 0x0, 0x1, 0x74};


#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_PMS7003_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#define SERIAL_TIMEOUT              1000

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_pms7003_status_e {
    SENSORDRIV_PMS7003_STAT_UNINITD = 0,
    SENSORDRIV_PMS7003_STAT_INITD,
    SENSORDRIV_PMS7003_STAT_STAT_READING,
} sensordriv_pms7003_status_t;

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

const can5_sensordriv_t sensordriv_pms7003 = {
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
            .read_warm_up = 30,
            .read_time = 60,
            .type = CAN5_SENSORDRIV_TYPE_PM,
        },
        .io_type = CAN5_PHY_IO_TYPE_UART,
        .name = "PMS7003",
        .version = "1.0",
        .manufacturer = "PlanTower",
    }
};

typedef struct pm_rx_buf_s {
    volatile bool start_found;
    char buf[PMS7003_MAX_LENGTH + 1];
    size_t len;
} pm_rx_buf_t;

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_pms7003_status_t status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_pms7003_data_t cb_data;
    can5_sensor_hwcb_f *hwcb;
    pm_rx_buf_t rx_buf;
    time_t read_time_end;
    struct {
        can5_sensor_pms7003_data_t data;
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

typedef enum cmd_type_e {
    MODIFY_COM_MODE,
    READ_DATA,
} cmd_type_t;
/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t __parse_read_rx(sensor_hdl_t *s_hdl, char buf, char *out_buf, cmd_type_t cmd_type);

static void __flush_uart_rx(sensor_hdl_t *s_hdl);

static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, can5_sensor_pms7003_data_t *pm_data, cmd_type_t cmd_type, time_t timeout);

static can5_err_t __read_pm(sensor_hdl_t *s_hdl, can5_sensor_pms7003_data_t *pm_data, cmd_type_t cmd_type);

static bool __validate_checksum(const char *data);
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
    s_hdl->status = SENSORDRIV_PMS7003_STAT_UNINITD;

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

    can5_sensor_pms7003_data_t pm_data;

    CLEAR_STRUCT(pm_data);

    s_hdl->port = port;

    if (__read_pm(s_hdl, &pm_data, MODIFY_COM_MODE) != CAN5_SUCCESS) {
        s_hdl->port = CAN5_PORT_NULL;
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    ESP_LOGI(TAG, "Detected PMS7003 PM module!");

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
    s_hdl->status = SENSORDRIV_PMS7003_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_PMS7003_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    can5_err_t ret;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_pms7003_data_t pm_data_inst;
        can5_sensor_pms7003_data_t *pm_data;
        CLEAR_STRUCT(pm_data_inst);

        ret = __read_pm(s_hdl, &pm_data_inst, READ_DATA);

        if (ret == CAN5_SUCCESS && pm_data_inst.read) {
            s_hdl->read.n++;

            for (can5_sensor_pms7003_type_t type = 0; type < PMS7003_TYPE_COUNT; type++) {
                s_hdl->read.data.pm[type].cf1 += (pm_data_inst.pm[type].cf1 - s_hdl->read.data.pm[type].cf1) / s_hdl->read.n;
                s_hdl->read.data.pm[type].atm_env += (pm_data_inst.pm[type].atm_env - s_hdl->read.data.pm[type].atm_env) /
                                                     s_hdl->read.n;
            }
            ESP_LOGI_V(TAG, "%d %d %d", s_hdl->read.data.pm[0].cf1, s_hdl->read.data.pm[1].cf1,
                     s_hdl->read.data.pm[2].cf1);

        }

        ESP_LOGI_V(TAG, "Read time %ld end time %ld", s_hdl->read_time_end, can5_time_ms(NULL));
        if (s_hdl->read_time_end <= can5_time_ms(NULL)) {
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_PMS7003_STAT_INITD;

            if (s_hdl->read.n > 0) {
                s_hdl->read.data.read = true;

                CLEAR_STRUCT(s_hdl->cb_data);
                pm_data = &s_hdl->cb_data;
                *pm_data = s_hdl->read.data;
                ESP_LOGI(TAG, "Averaged %d datapoints", s_hdl->read.n);

                s_hdl->readcb(get_sensor_id(hdl), pm_data, sizeof(*pm_data));
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
    if (s_hdl->status == SENSORDRIV_PMS7003_STAT_STAT_READING) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_PMS7003_STAT_STAT_READING;

    CLEAR_STRUCT(s_hdl->read.data);
    s_hdl->read.n = 0;

    s_hdl->read_time_end = read_time_end;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_PMS7003_STAT_INITD;
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
    sensor_hdl_t *s_hdl = hdl->hdl;

    // TODO: fix power management for CAN6
#ifdef CONFIG_IDF_TARGET_ESP32S3
    //hal.pm_enable(!enable);
#endif

    return hal.enable(enable, s_hdl->port);
}

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len)
{
    sensor_hdl_t *s_hdl = hdl->hdl;
    const can5_sensor_pms7003_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (!s_data->read) {
        goto done;
    }


    if (sizeof(can5_sensor_pms7003_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    /*
     * the following loops assume that CAN5_SENSOR_DATA_TYPE_PMS7003_* and PMS7003_TYPE_* are both in order of
     * pm1 pm2.5 and pm10
     */
    for (can5_sensor_pms7003_type_t type = 0; type < PMS7003_TYPE_COUNT; type++) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_CF1 + type, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC,
                                        NULL, 0, s_data->pm[type].cf1);
        if (elem) {
            TAILQ_INSERT_TAIL(list, elem, te);
        }

    }

    for (can5_sensor_pms7003_type_t type = 0; type < PMS7003_TYPE_COUNT; type++) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_ATM + type, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC,
                                        NULL, 0, s_data->pm[type].atm_env);
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

    return s_hdl->status >= SENSORDRIV_PMS7003_STAT_INITD;
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
    // TRACE_FUNC_SENSORDRIV_PMS7003; /* Too frequent */
    bool read_err = false;
    /* Consecutive "BM" = 0x42, 0x4d, means start of new packet */

    pm_rx_buf_t *rx_buf;
    rx_buf = &s_hdl->rx_buf;

    if (rx_buf->len + 1 > PMS7003_MAX_LENGTH + 1) {
        return CAN5_SENSOR_ERR_PARSE_ERROR;
    }

    if (rx_buf->start_found && rx_buf->len > 0 &&
        buf == PMS7003_START_BYTE2 && rx_buf->buf[rx_buf->len - 1] == PMS7003_START_BYTE1) {
        // we got consecutive "BM"
        rx_buf->buf[0] = PMS7003_START_BYTE1;
        rx_buf->len = 1;
    }

    if (!rx_buf->start_found && buf == PMS7003_START_BYTE1) {
        /* Check start byte */
        rx_buf->start_found = true;
        rx_buf->len = 0;
    }
    else if (rx_buf->start_found && rx_buf->len == (PMS7003_MAX_LENGTH - 1)) {
        /* Check end condition, i.e. length */
        rx_buf->buf[rx_buf->len++] = buf;  // store checksum
        rx_buf->start_found = false;
        if (__validate_checksum(rx_buf->buf)) {
            memcpy(out_buf, rx_buf->buf, rx_buf->len);
            rx_buf->len = 0;
            return CAN5_SUCCESS;
        }
        else {
            return CAN5_SENSOR_ERR_PARSE_ERROR;
        }
    }
    else if (cmd_type == MODIFY_COM_MODE && rx_buf->start_found && rx_buf->len == (sizeof (MODIFY_COMM_MODE_ACK) - 1)) {
        bool got = true;
        for (int i =0; i < rx_buf->len; i++) {
            if (rx_buf->buf[i] != MODIFY_COMM_MODE_ACK[i]) {
                got = false;
                break;
            }
        }
        if (got) {
            memcpy(out_buf, rx_buf->buf, rx_buf->len + 1);
            rx_buf->len = 0;
            return CAN5_SUCCESS;
        }
        else {
            return CAN5_SENSOR_ERR_PARSE_ERROR;
        }
    }

    /* add char to buffer */
    if (rx_buf->start_found) {
        rx_buf->buf[rx_buf->len++] = buf;
    }

    if (rx_buf->len == 1) {
        read_err = buf != PMS7003_START_BYTE2;
    }

    /* Check if max length is not crossed */
    read_err |= rx_buf->len >= PMS7003_MAX_LENGTH;

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

static can5_err_t __sensor_read_rx(sensor_hdl_t *s_hdl, can5_sensor_pms7003_data_t *pm_data, cmd_type_t cmd_type, time_t timeout)
{
    TRACE_FUNC;
    char buf;
    size_t len;
    char sentence[PMS7003_MAX_LENGTH];
    time_t start;

    CLEAR_ARRAY(sentence);


    start = can5_time_ms(NULL);

    while (can5_time_ms(NULL) <= start + timeout) {
        len = 1;
        hal.serial_recv(&buf, &len, s_hdl->port, SERIAL_TIMEOUT);
        if (len == 1) {
            if (__parse_read_rx(s_hdl, buf, sentence, cmd_type) == CAN5_SUCCESS) {
                pm_data->read = true;
                switch (cmd_type) {

                    case MODIFY_COM_MODE:
                        return CAN5_SUCCESS;
                    case READ_DATA:

                        /* CF=1 values */
                        pm_data->pm[PMS7003_TYPE_1].cf1 = (sentence[PMS7003_PM1_CF1_HIGH_POS] << 8) |
                                                          sentence[PMS7003_PM1_CF1_LOW_POS];
                        pm_data->pm[PMS7003_TYPE_2_5].cf1 = (sentence[PMS7003_PM2_5_CF1_HIGH_POS] << 8) |
                                                            sentence[PMS7003_PM2_5_CF1_LOW_POS];
                        pm_data->pm[PMS7003_TYPE_10].cf1 = (sentence[PMS7003_PM10_CF1_HIGH_POS] << 8) |
                                                           sentence[PMS7003_PM10_CF1_LOW_POS];

                        /* Atmospheric environment */
                        pm_data->pm[PMS7003_TYPE_1].atm_env = (sentence[PMS7003_PM1_ATE_HIGH_POS] << 8) |
                                                              sentence[PMS7003_PM1_ATE_LOW_POS];
                        pm_data->pm[PMS7003_TYPE_2_5].atm_env = (sentence[PMS7003_PM2_5_ATE_HIGH_POS] << 8) |
                                                                sentence[PMS7003_PM2_5_ATE_LOW_POS];
                        pm_data->pm[PMS7003_TYPE_10].atm_env = (sentence[PMS7003_PM10_ATE_HIGH_POS] << 8) |
                                                               sentence[PMS7003_PM10_ATE_LOW_POS];

                        ESP_LOG_BUFFER_HEXDUMP_V(TAG, sentence, PMS7003_MAX_LENGTH,  ESP_LOG_INFO);
                        return CAN5_SUCCESS;
                }
            }
        }
        taskYIELD();
    }

    return CAN5_SENSOR_ERR_PARSE_INCOMPLETE;
}


static can5_err_t __read_pm(sensor_hdl_t *s_hdl, can5_sensor_pms7003_data_t *pm_data, cmd_type_t cmd_type)
{
    TRACE_FUNC;

    __flush_uart_rx(s_hdl);

    //VERIFY_SUCCESS(hal.serial_send(READ_CO2_CMD, sizeof(READ_CO2_CMD), port, 100));
    switch (cmd_type) {

        case MODIFY_COM_MODE:
            VERIFY_SUCCESS(hal.serial_send(MODIFY_COMM_MODE_CMD, sizeof(MODIFY_COMM_MODE_CMD), s_hdl->port, SERIAL_TIMEOUT));
            break;
        case READ_DATA:
            VERIFY_SUCCESS(hal.serial_send(READ_PM_CMD, sizeof(READ_PM_CMD), s_hdl->port, SERIAL_TIMEOUT));
            break;

        default:
            return CAN5_ERR_INVALID_PARAM;
    }

    return __sensor_read_rx(s_hdl, pm_data, cmd_type, cmd_type == MODIFY_COM_MODE? 1000 : SERIAL_TIMEOUT);
}

static bool __validate_checksum(const char *data)
{
    uint16_t sum, packet_csum;

    sum = 0;
    for (size_t i = 0; i < PMS7003_MAX_LENGTH - 2; i++) {
        sum += data[i];
    }


    packet_csum = (data[PMS7003_CHECKSUM_HIGH_POS] << 8) | data[PMS7003_CHECKSUM_LOW_POS];

    ESP_LOGI_V(TAG, "checksum %x packet_csum %x", sum, packet_csum);

    if (packet_csum == sum) {
        return true;
    }

    return false;
}

/* ---------------------------------------------------------------------
 * Debug Support
 -----------------------------------------------------------------------*/

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief NEtif Status tags
 *
 */
static const can5_tag_tab_t _SENSORDRIV_PMS7003_STAT_STAT_tags = {
    TAG_TAB_ITEM(SENSORDRIV_PMS7003_STAT_UNINITD),
    TAG_TAB_ITEM(SENSORDRIV_PMS7003_STAT_INITD),
    TAG_TAB_ITEM(SENSORDRIV_PMS7003_STAT_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_PMS7003_STAT_STAT_tags);
}

#endif

/* ---------------------------------------------------------------------
 * Plugin Parser
 -----------------------------------------------------------------------*/

void parse_pm_read_data(const can5_sensordriv_t *driv, const can5_sensor_pms7003_data_t *pm_data, void *pout, size_t *plen)
{
    if (!pm_data->read) return;

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%d,%s:%d,%s:%d",
             can5_hazemon_get_type(HAZEMON_PM1_0)->token, pm_data->pm[PMS7003_TYPE_1].cf1,
             can5_hazemon_get_type(HAZEMON_PM2_5)->token, pm_data->pm[PMS7003_TYPE_2_5].cf1,
             can5_hazemon_get_type(HAZEMON_PM10)->token, pm_data->pm[PMS7003_TYPE_10].cf1);
    ESP_LOGI_V(TAG, "%s:%d,%s:%d,%s:%d",
               can5_hazemon_get_type(HAZEMON_PM1_0)->token, pm_data->pm[PMS7003_TYPE_1].cf1,
               can5_hazemon_get_type(HAZEMON_PM2_5)->token, pm_data->pm[PMS7003_TYPE_2_5].cf1,
               can5_hazemon_get_type(HAZEMON_PM10)->token, pm_data->pm[PMS7003_TYPE_10].cf1);
    *plen = strlen(pout);
}
