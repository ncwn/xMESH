/*******************************************************************************
* Author: @rmukhia
* Date:   7/11/22
*
* File:  can5_sensor_ws3226.c
* Descr: Weather Sensor ATTiny3226
*******************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "can5_hazemon_types.h"
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_hal.h"
#include "can5_sensor_ws3226.h"
#include "can5_config.h"

static const char *TAG = "SENSOR_WS3226";

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_WS3226_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

/* I2C address */
#define WS3226_SENSOR_ADDR                  0x57

/* Registers */
#define REG_STATUS                          0x00
/* Control Register */
#define REG_CR                              0x01
/* Wind direction */
#define REG_WD                              0x02
/* Wind speed */
#define REG_WS0                             0x03
#define REG_WS1                             0x04
#define REG_WS2                             0x05
/*  Rain */
#define REG_R0                              0x06
#define REG_R1                              0x07
#define REG_R2                              0x08
/* Battery Voltage */
#define REG_BV0                             0x09
#define REG_BV1                             0x0a

#define REG_SERIALNUM                       0x10
#define SERIALNUM_SIZE                      30



/* Others */
#define WS3226_CHIP_ID                      0x01
#define REG_CR_BIT_CRST                     7
#define REG_CR_BIT_PWRST                    6



typedef enum sensordriv_ws3226_status_e {
    SENSORDRIV_WS3226_STAT_UNINITD = 0,
    SENSORDRIV_WS3226_STAT_INITD,
    SENSORDRIV_WS3226_STAT_READING,
} sensordriv_ws3226_status_t;

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


const can5_sensordriv_t sensordriv_ws3226 = {
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
            .read_time = CAN5_SENSOR_TIME_MAX,
            .type = CAN5_SENSORDRIV_TYPE_WS3226,
        },
        .io_type = CAN5_PHY_IO_TYPE_I2C,
        .name = "WS3226",
        .version = "1.0",
        .manufacturer = "Interlab",
    }
};

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_ws3226_status_t  status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_ws3226_data_list_head_t cd_data_list;
    can5_sensor_hwcb_f *hwcb;
    char serial_num[SERIALNUM_SIZE];
    struct {
        time_t start;
        time_t end;
        time_t last_read;
    } read_time;
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
 * Function prototypes
 -----------------------------------------------------------------------*/

static can5_err_t __power_reset_sensor(sensor_hdl_t *s_hdl);
static can5_err_t __cycle_reset(sensor_hdl_t *s_hdl);

static can5_err_t __get_sensor_data(sensor_hdl_t *s_hdl, bool *new_reading, can5_sensor_ws3226_data_t *data);

// static can5_err_t __get_led(sensor_hdl_t *s_hdl, bool white, bool red, bool blue);
// static can5_err_t __set_led(sensor_hdl_t *s_hdl, bool white, bool red, bool blue);

static can5_err_t __read_sensor_single(sensor_hdl_t *s_hdl, can5_sensor_ws3226_data_t *data);

static can5_err_t __read_sensor_id(sensor_hdl_t *s_hdl);


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
    s_hdl->status = SENSORDRIV_WS3226_STAT_UNINITD;

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
    uint8_t chip_id;
    can5_err_t  ret;

    if ((ret = hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_STATUS, &chip_id, 1, I2C_MASTER_NACK)) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK_NO_ABORT(ret);
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    chip_id &= 0x7f;

    if (chip_id != WS3226_CHIP_ID){
        return CAN5_SENSOR_ERR_INVALID_PORT;
    }

    ESP_LOGI(TAG, "WS3226 DETECT SUCCESS | CHIP ID : 0x%X", chip_id);

    return CAN5_SUCCESS;
}



static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }

    /* Reset the sensor */
    VERIFY_SUCCESS(__power_reset_sensor(s_hdl));

    vTaskDelay(pdMS_TO_TICKS(300));
    /* Set time period for the sensor */
    //VERIFY_SUCCESS(__set_time_period(s_hdl, cycle_time_ms));

    __read_sensor_id(s_hdl);

    TAILQ_INIT(&s_hdl->cd_data_list);

    s_hdl->hwcb = hwcb;
    s_hdl->status = SENSORDRIV_WS3226_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_WS3226_STAT_UNINITD;

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    //TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    can5_sensor_ws3226_data_t tmp_data, *data;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {

        if (s_hdl->read_time.last_read + WS3226_POLL_INTERVAL < can5_time_ms(NULL)) {
            CLEAR_STRUCT(tmp_data);
            VERIFY_SUCCESS(__read_sensor_single(s_hdl, &tmp_data));
            VERIFY_SUCCESS(__cycle_reset(s_hdl));

            s_hdl->read_time.last_read = can5_time_ms(NULL);
            VERIFY_ALLOC(data, sizeof(can5_sensor_ws3226_data_t));
            *data = tmp_data;
            ESP_LOGI(TAG, "Collected datapoint at [%ld]", data->timestamp);
            TAILQ_INSERT_TAIL(&s_hdl->cd_data_list, data, te);
        }
        else {
            vTaskDelay(1);
        }

        if (s_hdl->read_time.end < can5_time_ms(NULL)) {
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_WS3226_STAT_INITD;

            if (!TAILQ_EMPTY(&s_hdl->cd_data_list)) {

                s_hdl->readcb(get_sensor_id(hdl), &s_hdl->cd_data_list, sizeof(intptr_t));
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
    can5_sensor_ws3226_data_t *curr, *next;

    s_hdl->status = SENSORDRIV_WS3226_STAT_READING;

    s_hdl->read_time.start = can5_time_ms(NULL);
    s_hdl->read_time.end = read_time_end;
    // past
    s_hdl->read_time.last_read = - WS3226_POLL_INTERVAL - 1;

    // free every element from list
    TAILQ_FOREACH_SAFE(curr, &s_hdl->cd_data_list, te, next) {
        TAILQ_REMOVE(&s_hdl->cd_data_list, curr, te);
        free(curr);
    }

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_WS3226_STAT_INITD;
        return CAN5_ERR_UNIMPLEMENTED;
    } else {
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
    const can5_sensor_ws3226_data_list_head_t *s_data = data;
    can5_sensor_ws3226_data_t *s_elem;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (TAILQ_EMPTY(s_data)) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    float rain_total = 0;
    float battery_average = 0;
    size_t total_n = 0;

    TAILQ_FOREACH(s_elem, s_data, te) {
        rain_total += s_elem->rain;
        battery_average += s_elem->battery_voltage;
        total_n++;
    }

    battery_average /= total_n;

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_WS3226_RAIN, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, rain_total);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, battery_average);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    TAILQ_FOREACH(s_elem, s_data, te) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_elem->wind_spd);
        if (elem) {
            TAILQ_INSERT_TAIL(list, elem, te);
        }
    }

    TAILQ_FOREACH(s_elem, s_data, te) {
        elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR, s_hdl->port,
                                        CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_elem->wind_dir);
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

    return s_hdl->status >= SENSORDRIV_WS3226_STAT_INITD;
}

static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request,const void *params, void *response)
{
    TRACE_FUNC;

    switch (request) {
        case SENSOR_WS3226_RESET:
            // reset the entire board
            __power_reset_sensor(hdl->hdl);
            break;
    }

    /* Let the mod set register */
    vTaskDelay(1);

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

/* total reboot of the ws3226 */
static can5_err_t __power_reset_sensor(sensor_hdl_t *s_hdl)
{
    uint8_t cr;
    VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_CR, &cr, 1, I2C_MASTER_NACK));
    cr |= BIT(REG_CR_BIT_PWRST);
    return hal.i2c_write_reg(WS3226_SENSOR_ADDR, REG_CR, &cr, 1, I2C_MASTER_NACK);
}

/* start new reading ws3226 */
static can5_err_t __cycle_reset(sensor_hdl_t *s_hdl)
{
    uint8_t cr;
    VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_CR, &cr, 1, I2C_MASTER_NACK));
    cr |= BIT(REG_CR_BIT_CRST);
    return hal.i2c_write_reg(WS3226_SENSOR_ADDR, REG_CR, &cr, 1, I2C_MASTER_NACK);
}

#if 0
static can5_err_t __get_read_size(sensor_hdl_t *s_hdl, int *read_size)
{
    uint8_t cr;
    VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_CR, &cr, 1, I2C_MASTER_NACK));
    *read_size = cr & 0x07;
    return CAN5_SUCCESS;
}

static can5_err_t __set_read_size(sensor_hdl_t *s_hdl, int read_size)
{
    uint8_t cr, tmp;
    VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_CR, &cr, 1, I2C_MASTER_NACK));
    tmp = (cr & ~(0x07)) | (read_size & 0x07);
    return hal.i2c_write_reg(WS3226_SENSOR_ADDR, REG_CR, &tmp, 1, I2C_MASTER_NACK);
}
#endif

static can5_err_t __get_sensor_data(sensor_hdl_t *s_hdl, bool *new_reading, can5_sensor_ws3226_data_t *data)
{
    static uint8_t reg[REG_BV1 + 1];
    uint16_t i_tmp1, i_tmp2;

    VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_STATUS, &reg[REG_STATUS], 1, I2C_MASTER_NACK));

    *new_reading = (reg[REG_STATUS] & 0x80) == 0x80;

    if (*new_reading) {
        data->timestamp = can5_time(NULL);

        for (int i = REG_WD; i <= REG_BV1; i++) {
            VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, i, &reg[i], 1, I2C_MASTER_NACK));
        }

        i_tmp1 = reg[REG_WD] & 0x0f;
        data->wind_dir = i_tmp1 * 22.5;

        // get the integer part
        i_tmp1 = ((reg[REG_WS2] & 0x03) << 8) | reg[REG_WS0];
        // get the fractional part
        i_tmp2 = ((reg[REG_WS2] & 0xFC) << 6) | reg[REG_WS1];
        // get float value
        data->wind_spd = i_tmp1 + (i_tmp2 * 0.0001f);


        // get the integer part
        i_tmp1 = ((reg[REG_R2] & 0x03) << 8) | reg[REG_R0];
        // get the fractional part
        i_tmp2 = ((reg[REG_R2] & 0xFC) << 6) | reg[REG_R1];
        // get float value
        data->rain = i_tmp1 + (i_tmp2 * 0.0001f);

        i_tmp1 = ((reg[REG_BV0] & 0xFF));
        i_tmp2 = ((reg[REG_BV1] & 0xFF));

        data->battery_voltage = i_tmp1 + (i_tmp2 * 0.01f);
    }

    ESP_LOG_BUFFER_HEXDUMP(TAG, reg, sizeof(reg), ESP_LOG_INFO);

    return CAN5_SUCCESS;
}

/*
static can5_err_t __get_led(sensor_hdl_t *s_hdl, bool white, bool red, bool blue)
{
    return CAN5_SUCCESS;
}

static can5_err_t __set_led(sensor_hdl_t *s_hdl, bool white, bool red, bool blue)
{
    return CAN5_SUCCESS;
}
*/

static can5_err_t __read_sensor_id(sensor_hdl_t *s_hdl)
{
    uint8_t serial_num[10]; // 10 bytes

    for (int i = 0; i < 10; i++) {
        VERIFY_SUCCESS(hal.i2c_read_reg(WS3226_SENSOR_ADDR, REG_SERIALNUM + i, &serial_num[i], 1, I2C_MASTER_NACK));
    }

    sprintf(s_hdl->serial_num, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
            serial_num[0],
            serial_num[1],
            serial_num[2],
            serial_num[3],
            serial_num[4],
            serial_num[5],
            serial_num[6],
            serial_num[7],
            serial_num[8],
            serial_num[9]);


    ESP_LOGI(TAG, "Serial number: %s", s_hdl->serial_num);

    return CAN5_SUCCESS;
}

static can5_err_t __read_sensor_single(sensor_hdl_t *s_hdl, can5_sensor_ws3226_data_t *data)
{
    bool new_reading;

    VERIFY_SUCCESS(__get_sensor_data(s_hdl, &new_reading, data));

    if (new_reading) {
        return CAN5_SUCCESS;
    }

    return CAN5_SENSOR_ERR_BUSY;
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
static const can5_tag_tab_t _SENSORDRIV_WS3226_STAT_tags = {
    TAG_TAB_ITEM(SENSORDRIV_WS3226_STAT_UNINITD),
    TAG_TAB_ITEM(SENSORDRIV_WS3226_STAT_INITD),
    TAG_TAB_ITEM(SENSORDRIV_WS3226_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;

    return TAG_LOOKUP(status, _SENSORDRIV_WS3226_STAT_tags);
}

#endif

void parse_ws3226_read_data(const can5_sensordriv_t *driv, const can5_sensor_ws3226_data_list_head_t *ws3226_data_list,
                            void *pout, size_t *plen)
{
    TRACE_FUNC;
    //if (!co2_data->read) return;
    can5_sensor_ws3226_data_t *data = TAILQ_FIRST(ws3226_data_list);

    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%.4f,%s:%.4f,%s:%.4f,%s:%.4f",
             can5_hazemon_get_type(HAZEMON_RAIN)->token, data->rain,
             can5_hazemon_get_type(HAZEMON_WIND_SPD)->token, data->wind_spd,
             can5_hazemon_get_type(HAZEMON_WIND_DIR)->token, data->wind_dir,
             can5_hazemon_get_type(HAZEMON_BATT_V)->token, data->battery_voltage);
    *plen = strlen(pout);
}

void get_ws3226_id(const can5_sensor_hdl_t *hdl, char **serial_num)
{
    sensor_hdl_t *s_hdl = hdl->hdl;
    *serial_num = strdup(s_hdl->serial_num);
}
