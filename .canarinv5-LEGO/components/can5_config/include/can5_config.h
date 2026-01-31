//
// Created by rmukhia on 23/12/21.
//

#ifndef _CAN5_CONFIG_SERVER_H_
#define _CAN5_CONFIG_SERVER_H_

#include "can5_error.h"
#include "can5_module.h"
#include "can5_types.h"

#define CAN5_TRUE_TOKEN                 "true"
#define CAN5_FALSE_TOKEN                "false"

typedef enum can5_cfg_type_e {
    CFG_INIT,                      // true if config is set, false otherwise
    CFG_OTA_MODE,                     // true if we are doing OTA updates.

    CFG_DEVICE_NAME,                    // AP mode show this as name
    CFG_DEVICE_ORGANIZATION,            // organization for this device
    CFG_DEVICE_ID,
    CFG_DEVICE_ID_POSTFIX,
    CFG_PROJECT,
    CFG_APP_VERSION,
    CFG_DEVICE_DATA_MODE,
    CFG_DATA_CYCLE_SEC,                 // seconds seperating two data collection cycle

    CFG_HAZEMON_ENABLE,
    CFG_HAZEMON_SYNC_INTERVAL,

    CFG_LORARELAY_ENABLE,
    CFG_LORARELAY_GPS_CYCLE,

    CFG_MQTT_CONFIGURATION_ENABLE,
    CFG_MQTT_DATA_ENABLE,
    CFG_MQTT_URI,
    CFG_MQTT_PORT,
    CFG_MQTT_USERNAME,
    CFG_MQTT_PASSWORD,
    CFG_MQTT_WAIT_FOR_ACK,
    CFG_MQTT_ENCRYPTED,
    CFG_MQTT_TLS_CERT,

    CFG_WIFI_AP_ENABLE,
    CFG_WIFI_AP_PASS,                  // password for Access Point

    CFG_WIFI_STA_ENABLE,
    CFG_WIFI_STA_SSID,                 // ssid for wifi STA -- connect to another wifi
    CFG_WIFI_STA_PASS,                 // password for wifi STA

    CFG_LWAN_ENABLE,

    CFG_LWAN_OTAA,                    // otaa or abp mode

    CFG_LWAN_DEVEUI,
    CFG_LWAN_APPEUI,

    CFG_LWAN_DADDR,
    CFG_LWAN_RX_1_DELAY,
    CFG_LWAN_RX_2_DELAY,

    CFG_LWAN_ADAPTIVE_DATA_RATE,
    CFG_LWAN_DATA_RATE,
    CFG_LWAN_TRANSMIT_POWER,
    CFG_LWAN_USER_DATALEN,
    CFG_LWAN_ADD_NUM_CYCLE_DATA,

    CFG_CELL_ENABLE,
    CFG_CELL_APN,                       // access point
    CFG_CELL_OPERATOR,
    CFG_CELL_ENABLE_GPS,

    CFG_ADC_0,                          // sensor driver for the port
    CFG_ADC_1,
    CFG_ADC_2,
    CFG_ADC_3,
    CFG_I2C_0,
    CFG_I2C_1,
    CFG_I2C_2,
    CFG_I2C_3,
    CFG_I2C_4,
    CFG_I2C_5,
    CFG_I2C_6,
    CFG_I2C_7,
    CFG_UART_0,
    CFG_UART_1,
    CFG_UART_2,
    CFG_UART_3,
    CFG_UART_4,
    CFG_UART_5,
    CFG_UART_6,
    CFG_UART_7,

    CFG_HIGH_FREQ_IMU,
    CFG_HIGH_FREQ_IMU_FREQ,


    CFG_LOG_TO_SD,                      // log to sd card
    CFG_LOG_TO_NETSOCK,                 // log to netsocket
    CFG_LOG_TO_NETSOCK_IP,
    CFG_LOG_TO_NETSOCK_PORT,

    CFG_HAZEMON_IP,                    // ip address of hazemon server
    CFG_HAZEMON_PORT,                  // UDP port to send sensor data to
    CFG_WIFI_SNTP_SERVER,              // secondary sntp server

    CFG_OTA_UPDATE_URL,


    CFG_LAST_G_LAT,
    CFG_LAST_G_LNG,
    CFG_LAST_G_ALT,
    CFG_LAST_G_TIME,


    CFG_LWAN_PKT_ID,
    CFG_LWAN_SEND_DELAY,
    CFG_LWAN_FW_CTX0,
    CFG_LWAN_FW_CTX1,
    CFG_LWAN_FW_CTX2,
    CFG_LWAN_FW_CTX3,
    CFG_LWAN_FW_CTX4,
    CFG_LWAN_FW_CTX5,
    CFG_LWAN_FW_CTX6,

    CFG_LWAN_STATS_SNR,
    CFG_LWAN_STATS_RSSI,
    CFG_LWAN_STATS_FCNT_UP,
    CFG_LWAN_STATS_FCNT_DOWN,
    CFG_LWAN_STATS_LAST_ACK,
    CFG_MQTT_STATS_LAST_ACK,

    CFG_CALIB_ZE07_CO_ADC_MULTI,
    CFG_CALIB_ZE07_CO_ADC_BIAS,
    CFG_CALIB_ZE07_CO_BIAS,

    CFG_COUNT,
    CFG_NULL __attribute__((unused)) = 0xFF,
} can5_cfg_type_t;

typedef struct can5_cfg_s {
    can5_module_t module;

    /* Read and write each element */
    can5_err_t (*write)(can5_cfg_type_t type, const uint8_t *data, const size_t len);
    can5_err_t (*read)(can5_cfg_type_t type, uint8_t **data, size_t *len);

    can5_err_t (*write_bool)(can5_cfg_type_t type, bool data);
    can5_err_t (*read_bool)(can5_cfg_type_t type, bool *data);

    can5_err_t (*write_int)(can5_cfg_type_t type, int64_t data);
    can5_err_t (*read_int)(can5_cfg_type_t type, int64_t *data);

    can5_err_t (*write_double)(can5_cfg_type_t type, double data);
    can5_err_t (*read_double)(can5_cfg_type_t type, double *data);

    can5_err_t (*remove)(can5_cfg_type_t type);


    /* Some persistent counter values */
    can5_err_t (*get_counter)(const char *key,int64_t *val);
    can5_err_t (*update_counter)(const char *key, bool increment);

    can5_err_t (*commit_config_to_disk)();

    can5_err_t (*factory_default)();


    /* Direct access to storage can be done using storage_manager */

} can5_cfg_t;

extern can5_cfg_t config_manager;

const char *can5_config_getstr(can5_cfg_type_t type);

#endif // _CAN5_CONFIG_SERVER_H_
