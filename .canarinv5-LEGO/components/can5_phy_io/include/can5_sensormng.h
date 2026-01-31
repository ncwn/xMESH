/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensormng.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSORMNG_H
#define CAN5_APP_CAN5_SENSORMNG_H
#include "esp_event.h"
#include "can5_module.h"
#include "can5_types.h"

typedef struct can5_sensormng_wake_up_time_s {
    time_t offset;
    uint8_t sensor_id;
} can5_sensormng_wake_up_time_t;

typedef struct can5_sensor_meta_details_s {
    const char *version;
    const char *name;
    const char *manufacturer;
    const char *last_reading;
    can5_port_idx_t port;
    can5_sensordriv_type_t type;
    char *serial_num;
} can5_sensor_meta_details_t;

typedef struct can5_sensormng_s {
    can5_module_t  module;

    /**
     * @brief Enable sensor with id.
     * @param enable enable or disable
     * @param sensor_id  sensor to enable
     * @return CAN5_SUCCESS
     */
    can5_err_t (*enable_sensor)(bool enable, uint8_t sensor_id);

    /**
     * @brief Return the maximum run time of sensors currently allocated.
     * @return time_t Maximum run of sensors from the sensors allocated.
     */
    time_t (*read_time)(void);

    /**
     * @brief Return the maximum warm up time of the sensors currently allocated.
     * @return time_t Maximum warm up time of the sensors from the sensors allocated.
     */
    time_t (*warm_up_time)(void);

    /**
     * @brief Descending sorted Array of warm up time of the sensors.
     * @param warm_up_array Out params. Descending sorted Array of warm up time.
     * @return CAN5_SUCCESS
     * @return can5_err_t on errors.
     */
    can5_err_t (*warm_up_time_array)(can5_sensormng_wake_up_time_t *wake_up, size_t *len);

    /**
     * @brief Get list of ready sensors.
     * @param type out. sensors list of ready sensors
     * @param len out. size of list.
     * @return can5_err_t
     */
    can5_err_t (*get_ready_sensors)(can5_sensor_meta_details_t *sensors_meta, size_t *len);

    /**
     * @brief Run driver control commands.
     * @return can5_err_t
     */
    can5_err_t (*run_sensor_commands)(const can5_sensordriv_type_t sensor_type, const int cmd, const void *params, void *response);

} can5_sensormng_t;

extern const can5_sensormng_t sensor_manager;

const char *can5_sensor_type_getstr(int type);

const char *sensor_hwevt_type_getstr(int type);

ESP_EVENT_DECLARE_BASE(CAN5_EVT_SENSORMNG);

#endif //CAN5_APP_CAN5_SENSORMNG_H
