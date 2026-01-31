/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensordriv.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSORDRIV_H
#define CAN5_APP_CAN5_SENSORDRIV_H

#include "can5_wiring.h"
#include "can5_sensor_data.h"
#include "can5_phy_io.h"

#define CAN5_SENSOR_PARSED_DATA_MAX_LEN  150
#define CAN5_SENSOR_STR_MAX_LEN     24

#define CAN5_SENSOR_TIME_MIN   0
#define CAN5_SENSOR_TIME_MAX   0xFFFF

typedef void (can5_sensor_readcb_f)(const uint8_t sensor_id, const void* read_data, const size_t len);

typedef enum can5_sensor_hw_evt_type_e {
    CAN5_SENSOR_HWEVT_NONE = 0x00,
    CAN5_SENSOR_HWEVT_UNIMPLIMENTED,
    CAN5_SENSOR_HWEVT_READING,
    CAN5_SENSOR_HWEVT_READ_FAILURE,
    CAN5_SENSOR_HWEVT_READ_COMPLETE,

    CAN5_SENSOR_HWEVT_LAST,
} can5_sensor_hw_evt_type_t;


typedef struct can5_sensor_hw_evt_s {
    can5_sensor_hw_evt_type_t type;
    uint8_t sensor_id;
} can5_sensor_hw_evt_t;

// callback for sensor hardware events
typedef void (can5_sensor_hwcb_f)(const can5_sensor_hw_evt_t *evt);



typedef struct can5_sensor_hdl_s {
    void *hdl;                               // hdl pointer
    char hdl_data[];                         // hdl data
} can5_sensor_hdl_t;

/**
 * @brief Driver for sensors
 *
 */
typedef struct can5_sensordriv_ops_s {

    // allocate a sensor handle
    can5_sensor_hdl_t *(*alloc)(size_t *len);

    uint8_t (*get_id)(can5_sensor_hdl_t *hdl);

    void (*set_id)(can5_sensor_hdl_t *hdl, uint8_t sensor_id);

    // return 0 if detected, and save details in the specified location
    can5_err_t (*detect)(can5_sensor_hdl_t *hdl, can5_port_idx_t port);

    // initialize the module to be ready to read
    can5_err_t (*init)(can5_sensor_hdl_t *hdl, can5_port_idx_t port, can5_sensor_hwcb_f *hwevt_cb);

    // deinitialize the module
    can5_err_t (*uninit)(can5_sensor_hdl_t *hdl);

    // rune once the card main function
    can5_err_t (*run)(can5_sensor_hdl_t *hdl);

    /**
     * @brief Read len bytes from the driver and store data into prxdata buffer
     * @param prxdata pointer to return buffer
     * @param plen    pointer to size variable. It'll be used for number of byte to receive;
     * @param blocking  Call is blocking or non-blocking.
     *
     */
    can5_err_t (*read)(can5_sensor_hdl_t *hdl, void* prxdata, size_t* plen, time_t timeout, bool blocking);

    // set the callback function to execute upon reading on a specific port
    can5_err_t (*register_read_cb)(can5_sensor_hdl_t *hdl, can5_sensor_readcb_f*);

    bool       (*is_running)(can5_sensor_hdl_t *hdl);

    can5_err_t (*enable)(can5_sensor_hdl_t *hdl, bool enable);

    /**
     * @brief Get unified can5_sensor_data_t type from the result of read.
     * @param hdl The sensor handle.
     * @param read_data The sensor data read.
     * @param read_len The sensor data len.
     * @return can5_sensor_data_list_t, NULL on error
     */
    can5_sensor_data_list_t *(*get_sensor_data)(can5_sensor_hdl_t *hdl, const void *read_data, size_t read_len);

    /**
     * @brief run i/o commands
     * @param request The request integer
     * @param params  The params
     * @param response Response to this command
     */
    can5_err_t (*driverctl)(can5_sensor_hdl_t *hdl, uint8_t request, const void *params, void *response);

    // Get driver status
    int32_t  (*status_get)(can5_sensor_hdl_t *hdl);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0
    // print status in human readable format for debug
    const char*  (*status_getstr)(can5_sensor_hdl_t *hdl, int32_t status);
#endif
} can5_sensordriv_ops_t;

#define CAN5_SENSOR_ID_NONE  0xFF

typedef struct can5_sensordriv_s {
    can5_sensordriv_ops_t ops;
    can5_phy_io_details_t details;
} can5_sensordriv_t;

#endif //CAN5_APP_CAN5_SENSORDRIV_H
