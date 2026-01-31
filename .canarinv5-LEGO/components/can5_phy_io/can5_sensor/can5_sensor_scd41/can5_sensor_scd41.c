/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.c
* Descr: The driver needs a task because it takes 5 second to read data from scd41
*        and without a task, the driver will block the main thread for 5 seconds
*        hampering other sensors.
*******************************************************************************/
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "esp_log.h"
#include "can5_error.h"
#include "can5_utils.h"
#include "can5_sensor_scd41.h"
#include "can5_hazemon_types.h"
#include "sensirion_i2c_hal.h"
#include "scd4x_i2c.h"

#define TAG             "SENSOR_SCD41"
#define READ_TIME       5000        // 5 seconds for scd41 to read data as mentioned in datasheet
#define REFRESH_READ    BIT(0)

#define CHECK_INITD(s_hdl) do {                       \
    if ((s_hdl)->status == SENSORDRIV_SCD41_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

typedef enum sensordriv_scd41_status_e {
    SENSORDRIV_SCD41_STAT_UNINITD = 0,
    SENSORDRIV_SCD41_STAT_INITD,
    SENSORDRIV_SCD41_STAT_READING,
    SENSOR_SCD41_STAT_CALIBRATING,
} sensordriv_scd41_status_t;

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


const can5_sensordriv_t sensordriv_scd41 = {
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
            .type = CAN5_SENSORDRIV_TYPE_SCD41,
        },
        .io_type = CAN5_PHY_IO_TYPE_I2C,
        .name = "SCD41",
        .version = "1.0",
        .manufacturer = "Sensirion",
    }
};

typedef struct sensor_hdl_s {
    uint8_t sensor_id;
    volatile sensordriv_scd41_status_t status;
    volatile bool async_read;
    can5_port_idx_t port;
    can5_sensor_readcb_f *readcb;
    can5_sensor_scd41_data_t cb_data;
    can5_sensor_hwcb_f *hwcb;
    time_t read_time_end;
    TaskHandle_t task;
    SemaphoreHandle_t sem;
    struct {
        can5_sensor_scd41_data_t data;
        ssize_t n;
        can5_sensor_scd41_data_t cache_data; // scd41 can only read once in 5 seconds, the cache data is used to store the data for the next 5 seconds
        time_t last_read_time;
    } read;
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
portTASK_FUNCTION(__scd41_task, pvParameters);

esp_err_t __update_reading(can5_sensor_hdl_t *hdl, can5_sensor_scd41_data_t *scd41_data)
{
    sensor_hdl_t *s_hdl = hdl->hdl;

    can5_sensor_scd41_data_t data;
    //if (scd4x_measure_single_shot() != 0) {
    //    goto err;
    //}
    bool data_ready = false;
    while (!data_ready) {
        if (scd4x_get_data_ready_flag(&data_ready) != 0)
            goto err;
        vTaskDelay(pdMS_TO_TICKS(100));
    }


    int32_t temp, humi;
    if (scd4x_read_measurement(&data.co2,
                               &temp,
                               &humi) != 0) {
        goto err;
    }
    data.temperature = temp / 1000.0;
    data.humidity = humi / 1000.0;
    data.read = true;

    //xSemaphoreTake(s_hdl->sem, portMAX_DELAY);
    *scd41_data = data;
    //xSemaphoreGive(s_hdl->sem);

    return CAN5_SUCCESS;
err:
    return CAN5_SENSOR_ERR_TIMEOUT;
}

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
    s_hdl->status = SENSORDRIV_SCD41_STAT_UNINITD;

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

    enable(hdl, true);
    uint16_t serial_words[3];
    uint64_t serial = 0;

    scd4x_stop_periodic_measurement();

    if (scd4x_get_serial_number(&serial_words[0], &serial_words[1], &serial_words[2]) != 0) {
        return CAN5_HAL_ERR_INVALID_PORT;
    }

    serial = (serial_words[2] << 32) | (serial_words[1] << 16) | serial_words[0];

    ESP_LOGI(TAG, "serial %llu", serial);
    enable(hdl, false);
    return CAN5_SUCCESS;
}

static can5_err_t init(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwcb)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;
    uint16_t serial_words[3];

    if (s_hdl->port != port) {
        VERIFY_SUCCESS(detect(hdl, port));
    }



    s_hdl->hwcb = hwcb;
    s_hdl->port = port;
    xTaskCreate(__scd41_task, "scd41_task", 2048, hdl, 10, &s_hdl->task);
    configASSERT(s_hdl->task);

    s_hdl->sem = xSemaphoreCreateBinary();
    configASSERT(s_hdl->sem);

    //scd4x_stop_periodic_measurement();
    scd4x_start_periodic_measurement();
    //int32_t offset = 2000;
    //scd4x_set_temperature_offset(offset);
    //ESP_LOGI(TAG, "temperature offset %d", offset);

    s_hdl->status = SENSORDRIV_SCD41_STAT_INITD;

    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    s_hdl->port = CAN5_PORT_NULL;

    s_hdl->status = SENSORDRIV_SCD41_STAT_UNINITD;
    scd4x_stop_periodic_measurement();

    return CAN5_SUCCESS;
}

static can5_err_t run(can5_sensor_hdl_t *hdl)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    if (s_hdl->async_read) {
        can5_sensor_scd41_data_t *scd41_data_inst = &s_hdl->read.cache_data;
        can5_sensor_scd41_data_t *scd41_data;

        if (s_hdl->status == SENSORDRIV_SCD41_STAT_READING) {
            xTaskNotify(s_hdl->task, REFRESH_READ, eSetBits);
        }

        //xSemaphoreTake(s_hdl->sem, portMAX_DELAY);
        if (scd41_data_inst->read) {
            s_hdl->read.n++;
            s_hdl->read.data.co2 += (scd41_data_inst->co2 - s_hdl->read.data.co2) / s_hdl->read.n;
            s_hdl->read.data.humidity += (scd41_data_inst->humidity - s_hdl->read.data.humidity) /s_hdl->read.n;
            s_hdl->read.data.temperature += (scd41_data_inst->temperature - s_hdl->read.data.temperature) /s_hdl->read.n;
            s_hdl->read.cache_data.read = false;
            scd41_data_inst->read = false;
        }

        //xSemaphoreGive(s_hdl->sem);

        if (s_hdl->read_time_end <= can5_time_ms(NULL)) {
            s_hdl->async_read = false;
            s_hdl->status = SENSORDRIV_SCD41_STAT_INITD;

            if (s_hdl->read.n > 0) {
                CLEAR_STRUCT(s_hdl->cb_data);
                scd41_data = &s_hdl->cb_data;
                *scd41_data = s_hdl->read.data;
                ESP_LOGI(TAG, "Averaged %d datapoints", s_hdl->read.n);

                s_hdl->readcb(get_sensor_id(hdl), scd41_data, sizeof(*scd41_data));
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
    if (s_hdl->status != SENSORDRIV_SCD41_STAT_INITD) {
        return CAN5_SENSOR_ERR_BUSY;
    }

    s_hdl->status = SENSORDRIV_SCD41_STAT_READING;

    CLEAR_STRUCT(s_hdl->read.data);
    s_hdl->read.n = 0;

    s_hdl->read_time_end = read_time_end;

    if (blocking) {
        SENSOR_HW_EVT(hdl, CAN5_SENSOR_HWEVT_UNIMPLIMENTED);
        s_hdl->status = SENSORDRIV_SCD41_STAT_INITD;
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
    TRACE_FUNC;
    return ESP_OK;
#if 0
    ESP_LOGI(TAG, "enable %d", enable);
    can5_err_t ret;
    if (enable) {
        ret = scd4x_wake_up() == 0? CAN5_SUCCESS : CAN5_HAL_ERR_INVALID_PORT;
    }
    else {
        ret = scd4x_power_down() == 0? CAN5_SUCCESS : CAN5_HAL_ERR_INVALID_PORT;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ret;
#endif
}

static can5_sensor_data_list_t *get_sensor_data(can5_sensor_hdl_t *hdl, const void *data, size_t data_len)
{
    sensor_hdl_t *s_hdl = hdl->hdl;
    const can5_sensor_scd41_data_t *s_data = data;
    can5_sensor_data_list_t *list;
    can5_sensor_data_t *elem;

    list = NULL;

    if (!s_data) {
        goto done;
    }

    if (sizeof(can5_sensor_scd41_data_t) != data_len) {
        goto done;
    }

    list = calloc(sizeof (can5_sensor_data_list_t), 1);
    if (!list) {
        goto done;
    }

    TAILQ_INIT(list);

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_SCD41_CO2, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->co2);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_SCD41_TEMP, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->temperature);
    if (elem) {
        TAILQ_INSERT_TAIL(list, elem, te);
    }

    elem = can_5_sensor_data_create(CAN5_SENSOR_DATA_TYPE_SCD41_HUMI, s_hdl->port,
                                    CAN5_SENSOR_DATA_DATATYPE_DEC, NULL, 0, s_data->humidity);
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

    return s_hdl->status >= SENSORDRIV_SCD41_STAT_INITD;
}


static can5_err_t driverctl(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    CHECK_INITD(s_hdl);

    switch (request) {
    case SENSOR_SCD41_FORCE_CALIBRATE:
        {
            uint16_t target_co2 = *(uint16_t *)params;
            uint16_t *resp = (uint16_t *)response;
            s_hdl->status = SENSOR_SCD41_STAT_CALIBRATING;
            ESP_LOGI(TAG, "Starting calibration...");
            scd4x_stop_periodic_measurement();
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Calibrating with target CO2: %d", target_co2);
            if (scd4x_perform_forced_recalibration(target_co2, resp) != 0)
            {
                return CAN5_SENSOR_ERR_PARSE_ERROR;
            }

            if (*resp == 0xFFFF) {
                ESP_LOGE(TAG, "Calibration failed. Response: %d", *resp);
                return CAN5_SENSOR_ERR_INVALID_PORT;
            }
            ESP_LOGI(TAG, "Calibration done. Response: %d", *resp);
            break;
        }default:
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

portTASK_FUNCTION(__scd41_task, pvParameters)
{
    can5_sensor_hdl_t *hdl = (can5_sensor_hdl_t *)pvParameters;
    sensor_hdl_t *s_hdl = hdl->hdl;
    uint32_t notify_result;
    while(true) {

        if (xTaskNotifyWait(0, ULONG_MAX, &notify_result, portMAX_DELAY) == pdTRUE) {
            if (notify_result & REFRESH_READ) {
                time_t start_reading = can5_time_ms(NULL);
                // skip if the sensor has been read in the last 5 seconds
                if (start_reading < s_hdl->read.last_read_time + READ_TIME) {
                    continue;
                }
                if (__update_reading(hdl, &s_hdl->read.cache_data) == CAN5_SUCCESS) {
                    s_hdl->read.last_read_time = start_reading;
                }
            }
        }

    }

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
static const can5_tag_tab_t _SENSORDRIV_SCD41_STAT_tags = {
        TAG_TAB_ITEM(SENSORDRIV_SCD41_STAT_UNINITD),
        TAG_TAB_ITEM(SENSORDRIV_SCD41_STAT_INITD),
        TAG_TAB_ITEM(SENSORDRIV_SCD41_STAT_READING),
};


static const char *status_getstr(can5_sensor_hdl_t *hdl, int32_t status)
{
    TRACE_FUNC;
    sensor_hdl_t *s_hdl = hdl->hdl;

    return TAG_LOOKUP(s_hdl->status, _SENSORDRIV_SCD41_STAT_tags);
}

#endif
void parse_scd41_read_data(const can5_sensordriv_t *driv, const can5_sensor_scd41_data_t *scd41_data, void *pout, size_t *plen)
{
    memset(pout, 0, CAN5_SENSOR_PARSED_DATA_MAX_LEN);
    snprintf(pout, CAN5_SENSOR_PARSED_DATA_MAX_LEN, "%s:%d,%s:%f,%s:%f",
             can5_hazemon_get_type(HAZEMON_MHZ16_CO2)->token, scd41_data->co2,
             can5_hazemon_get_type(HAZEMON_HUMIDITY)->token, scd41_data->humidity,
             can5_hazemon_get_type(HAZEMON_TEMPERATURE)->token, scd41_data->temperature);
    *plen = strlen(pout);
}
