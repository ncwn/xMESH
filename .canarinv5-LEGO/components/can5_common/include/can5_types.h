/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can5_types.cpp
 * Descr: Definition of smalled tyoes tailored to can5 small messages application
 *******************************************************************************/

#ifndef __CAN5_TYPES_H__
#define __CAN5_TYPES_H__

#include <stdint.h>

// #define seconds *1000
#define CAN5_STORAGE_MAX_LEN    1500
#define CAN5_IO_PORT_MAX        20      // 18 io ports in board


/* define types here to remove circular dependencies */


typedef enum can5_port_idx_e {
    /** Net-ports start */
    NETPORT_0 = 0x00,  // WIFI
    NETPORT_1,         // PCIe mini (lora/3g)
    /** ad-ports end */
    NETPORT_COUNT,

    /** AD-ports start */
    ADPORT_0 = NETPORT_COUNT,
    ADPORT_1,
    ADPORT_2,
    ADPORT_3,
    /** ad-ports end */
    ADPORT_COUNT,

    /** U-ports start */
    UPORT_0 = ADPORT_COUNT,
    UPORT_1,
    UPORT_2,
    UPORT_3,
    UPORT_4,
    UPORT_5,
    UPORT_6,
    UPORT_7,
    /** U-ports end */
    UPORT_COUNT,

    CAN5_PORT_COUNT = UPORT_COUNT,
    I2C_0,
    CAN5_PORT_NULL = 0xFF
} can5_port_idx_t;

typedef enum can5_sensordriv_type_e {
    CAN5_SENSORDRIV_TYPE_PM,
    CAN5_SENSORDRIV_TYPE_CO2,
    CAN5_SENSORDRIV_TYPE_GPS,
    CAN5_SENSORDRIV_TYPE_CO_ADC,
    CAN5_SENSORDRIV_TYPE_CO,
    CAN5_SENSORDRIV_TYPE_BME680,
    CAN5_SENSORDRIV_TYPE_BME280,
    CAN5_SENSORDRIV_TYPE_NO2,
    CAN5_SENSORDRIV_TYPE_SIM7600_GPS,
    CAN5_SENSORDRIV_TYPE_WS3226,
    CAN5_SENSORDRIV_TYPE_MPU6500,
    CAN5_SENSORDRIV_TYPE_SCD41,
    /* Add more sensor types here */

    CAN5_SENSORDRIV_TYPE_COUNT,
    CAN5_SENSORDRIV_TYPE_NONE = 0xFF,
} can5_sensordriv_type_t;

typedef enum can5_actuatordriv_type_e {
    CAN5_ACTUATORDRIV_TYPE_RELAY,

    CAN5_ACTUATORDRIV_TYPE_COUNT,
    CAN5_ACTUATORDRIV_TYPE_NONE = 0xFF,
} can5_actuatordriv_type_t;

typedef enum can5_storagedriv_type_e {
    CAN5_STORAGE_TYPE_SDSPI = 0,
    CAN5_STORAGE_TYPE_RAM,
    CAN5_STORAGE_TYPE_NVS,
    CAN5_STORAGE_TYPE_COUNT, // max
    CAN5_STORAGE_TYPE_NONE= 0xFF,
} can5_storagedriv_type_t;

// interface type used to identify comunication modules and link to the relative libraries
typedef enum can5_netif_type_e {
    CAN5_NET_TYPE_LWIP    = 0,      // we can use normal tcp/udp sockets.
    CAN5_NET_TYPE_CELL       ,
    CAN5_NET_TYPE_LORAWAN   ,
    /* add more interface types above here (keel CAN5_NET_SUPPORTED_IF as last item and update the value)*/
    CAN5_NET_TYPE_COUNT,  // keep as last item (used to count)
    CAN5_NET_TYPE_NONE = 0xFF
} can5_netif_type_t;

// device modes
typedef enum can5_device_mode_t {
    CAN5_MODE_NORMAL,
    CAN5_MODE_CONFIG,
    CAN5_MODE_COUNT,  // keep as last item (used to count)
} can5_device_mode_t;

/**
 * @brief Different net communication strategies available to network manager.
 */
typedef enum can5_netproto_type_e {
    CAN5_NETPROTO_HAZEMON,
    CAN5_NETPROTO_LORARELAY,
    CAN5_NETPROTO_MQTT,

    CAN5_NETPROTO_COUNT,
    CAN5_NETPROTO_NONE,
} can5_netproto_type_t;

typedef enum can5_device_data_mode_e {
    CAN5_DATA_MODE_CYCLE,
    CAN5_DATA_MODE_REALTIME,
} can5_device_data_mode_t;

const char *can5_sensor_type_getstr(int type);
const char *can5_actuator_type_getstr(can5_actuatordriv_type_t type);
const char *can5_storagedriv_type_getstr(can5_storagedriv_type_t type);
const char *can5_hal_port_getstr(int32_t evt);
const char* can5_netif_getstr(can5_netif_type_t type);
const char* can5_device_mode_getstr(can5_device_mode_t mode);
const char* can5_netproto_type_get_str(can5_netproto_type_t type);
const char* can5_device_data_mode_get_str(can5_device_data_mode_t type);

#endif // CAN5_TYPES_H