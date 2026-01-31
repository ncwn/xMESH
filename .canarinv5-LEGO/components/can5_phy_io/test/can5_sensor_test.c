#include <can5_hal.h>
#include <can5_sensor_mh_z16_co2.h>
#include "string.h"
#include "can5_error.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "can5_sensordriv.h"
#include "can5_sensor_co.h"
#include "can5_sensor_ublox_neo_gps.h"
#include "nmea.h"
#include "can5_sensor_bme280.h"

static const char *TAG="SENSOR_TEST";

#define CHECK_RESULT(r) do { \
int ret;  \
ret = r;  \
if (ret == CAN5_SUCCESS) ESP_LOGI(TAG, "OK!");  else CAN5_ERR_CHECK_NO_ABORT(ret);   \
} while (0)


#define CHECK_RESULT_FAIL(r) do { \
int ret;  \
ret = r;  \
if (ret != CAN5_SUCCESS) ESP_LOGI(TAG, "OK Failed %d!", ret);  \
} while (0)


bool co_read_cb_done;

void co_read_cb(const uint8_t sensor_id, const void *read_data, const size_t len)
{
    ESP_LOG_BUFFER_HEXDUMP(TAG, read_data, len, ESP_LOG_INFO);
    co_read_cb_done = true;
}

can5_err_t sensordriv_test_co()
{
    static char data[255];
    memset(data, 0, sizeof(char) * 255);
    size_t len;
    ESP_LOGI(TAG, "CO Sensor Test Start!");
    CHECK_RESULT(sensordriv_co.ops.detect(ADPORT_1));
    CHECK_RESULT(sensordriv_co.ops.init(ADPORT_1, NULL));
    ESP_LOGI(TAG, "Blocking read!");
    CHECK_RESULT(sensordriv_co.ops.read(data, &len, true));
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO);

    CHECK_RESULT(sensordriv_co.ops.register_read_cb(co_read_cb));
    ESP_LOGI(TAG, "Non-Blocking read!");
    co_read_cb_done = false;
    CHECK_RESULT(sensordriv_co.ops.read(NULL, NULL, false));

    while (!co_read_cb_done) {
        sensordriv_co.ops.run();
        vTaskDelay(100);
    }

    CHECK_RESULT(sensordriv_co.ops.uninit());
    ESP_LOGI(TAG, "CO Sensor Test End!");
    return CAN5_SUCCESS;
}

bool gps_read_cb_done;

void gps_read_cb(const uint8_t sensor_id, const void *read_data, const size_t len)
{
    sensordriv_ublox_neo.ops.driverctl(CAN5_SENSOR_GPS_DRIVERCTL_PRINT, read_data, NULL);
    gps_read_cb_done = true;
}

can5_err_t sensordriv_test_ublox_neo_m8m()
{
    size_t len;
    can5_gps_data_t pdata;
    can5_port_idx_t  port = UPORT_4;

    ESP_LOGI(TAG, "UBLOX NEO M8M Sensor Test Start!");
    hal.enable(1, port);
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_LOGI(TAG, "Port %s", can5_hal_port_getstr(port));
    CHECK_RESULT(sensordriv_ublox_neo.ops.detect(port));
    CHECK_RESULT(sensordriv_ublox_neo.ops.init(port, NULL));
    ESP_LOGI(TAG, "Blocking read!");
    CHECK_RESULT(sensordriv_ublox_neo.ops.read(&pdata, &len, true));
    CHECK_RESULT(sensordriv_ublox_neo.ops.driverctl(CAN5_SENSOR_GPS_DRIVERCTL_PRINT, &pdata, NULL));

    CHECK_RESULT(sensordriv_ublox_neo.ops.register_read_cb(gps_read_cb));
    ESP_LOGI(TAG, "Non-Blocking read!");
    gps_read_cb_done = false;
    CHECK_RESULT(sensordriv_ublox_neo.ops.read(NULL, NULL, false));

    while (!gps_read_cb_done) {
        sensordriv_ublox_neo.ops.run();
        vTaskDelay(100);
    }

    CHECK_RESULT(sensordriv_ublox_neo.ops.uninit());
    ESP_LOGI(TAG, "UBLOX NEO M8M Sensor Test End!");
    return CAN5_SUCCESS;
}

bool co2_read_cb_done;

void co2_read_cb(const uint8_t sensor_id, const void *read_data, const size_t len)
{
    const can5_co2_data_t *data = read_data;
    ESP_LOGI(TAG, "CO2 data: %u ppm", data->value);
    co2_read_cb_done = true;
}

can5_err_t sensordriv_test_mhz16()
{
    size_t len;
    can5_co2_data_t pdata;
    can5_port_idx_t  port = UPORT_3;
    can5_err_t ret;
    ESP_LOGI(TAG, "MH-Z16 Sensor Test Start!");
    ret = sensordriv_mhz16.ops.detect(port);
    CAN5_ERR_CHECK(ret);
    CHECK_RESULT(sensordriv_mhz16.ops.init(port, NULL));
    ESP_LOGI(TAG, "Blocking read!");
    CHECK_RESULT(sensordriv_mhz16.ops.read(&pdata, &len, true));
    ESP_LOGI(TAG, "CO2 data: %u ppm", pdata.value);

    CHECK_RESULT(sensordriv_mhz16.ops.register_read_cb(co2_read_cb));
    ESP_LOGI(TAG, "Non-Blocking read!");
    co2_read_cb_done = false;
    CHECK_RESULT(sensordriv_mhz16.ops.read(NULL, NULL, false));

    while (!co2_read_cb_done) {
        sensordriv_mhz16.ops.run();
        vTaskDelay(100);
    }

    CHECK_RESULT(sensordriv_mhz16.ops.uninit());
    ESP_LOGI(TAG, "MH-Z16 Sensor Test End!");
    return CAN5_SUCCESS;
}

can5_err_t sensordriv_test_bme280(){

    size_t len;
    can5_sensor_bme280_data_t pdata;

    ESP_LOGI(TAG, "BME280 Sensor Test Start!");
    CHECK_RESULT(sensordriv_bme280.ops.detect(CAN5_PORT_NULL));
    CHECK_RESULT(sensordriv_bme280.ops.driverctl(SENSOR_BME280_RESET, NULL, NULL));
    CHECK_RESULT(sensordriv_bme280.ops.driverctl(SENSOR_BME280_FORCED_MODE, NULL, NULL));
    CHECK_RESULT(sensordriv_bme280.ops.init(CAN5_PORT_NULL, NULL));
    ESP_LOGI(TAG, "Blocking read!");
    CHECK_RESULT(sensordriv_bme280.ops.read(&pdata, &len, true));
    ESP_LOGI(TAG, "BME280 Sensor Test Done!");

    //sensordriv_bme280.ops.driverctl(SENSOR_BME280_MODE3, NULL, NULL);
    return CAN5_SUCCESS;
}
