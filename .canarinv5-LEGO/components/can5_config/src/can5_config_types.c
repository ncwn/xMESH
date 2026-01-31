/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   11/01/22
*
* File:  can5_config_types.c
* Descr:
*******************************************************************************/

#include "stddef.h"
#include "can5_utils.h"
#include "can5_config_types.h"
#include "can5_storage_fsfat.h"

#define CFG_ITEM(_type, _tag, _storage_type,  _max_len, _val)  {     \
    .type = _type,                                          \
    .tag = _tag,                                            \
    .storage_type = _storage_type,                          \
    .default_val = _val,                                    \
    .max_len = _max_len,                                    \
}

#define CFG_TAG(_tag, _wear_leveling) {                    \
    .tag = _tag,                                            \
    .wear_leveling = _wear_leveling,                        \
}

#define SENSOR_NULL         ""

#define TAG_FS              CAN5_CFG_STORAGE_TYPE_TAG_FS
#define RAW_FS              CAN5_CFG_STORAGE_TYPE_RAW_FS

#ifndef CAN5_LINUX_HOST_TEST
#define NVS                 CAN5_CFG_STORAGE_TYPE_NVS
#else
#define NVS                 TAG_FS //CAN5_STORAGE_TYPE_NVS
#endif
#define RAM                 CAN5_CFG_STORAGE_TYPE_RAM

/* Tags based config */
static const char * const GENERAL_TAG           = "general";
static const char * const COMMS_TAG             = "comms";
static const char * const SENSORS_TAG           = "sensors";
static const char * const NETIF_TAG             = "netif";
static const char * const LOG_TAG               = "log";
static const char * const GPS_TAG               = "gps";
static const char * const VAR_TAG               = "var";
static const char * const CALIBRATION_TAG       = "calib";

/* Files based config */
static const char * const LORAWAN_CTX0          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan0";
static const char * const LORAWAN_CTX1          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan1";
static const char * const LORAWAN_CTX2          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan2";
static const char * const LORAWAN_CTX3          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan3";
static const char * const LORAWAN_CTX4          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan4";
static const char * const LORAWAN_CTX5          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan5";
static const char * const LORAWAN_CTX6          = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/lwan6";
static const char * const MQTT_TLS_CERT         = CAN5_STORAGE_FSFAT_MOUNT_DIR "/run/mqtt_crt";

static const can5_cfg_meta_tags_t default_tags[] = {
    CFG_TAG(GENERAL_TAG,        true),
    CFG_TAG(COMMS_TAG,          true),
    CFG_TAG(SENSORS_TAG,        true),
    CFG_TAG(NETIF_TAG,          true),
    CFG_TAG(LOG_TAG,            true),
    CFG_TAG(GPS_TAG,            true),
    CFG_TAG(VAR_TAG,            true),
    CFG_TAG(CALIBRATION_TAG,    true),

};

static const char * raw_files_list[] = {
    LORAWAN_CTX0,
    LORAWAN_CTX1,
    LORAWAN_CTX2,
    LORAWAN_CTX3,
    LORAWAN_CTX4,
    LORAWAN_CTX5,
    LORAWAN_CTX6,
    MQTT_TLS_CERT,
};

#define BOOLEAN_LEN     6
#define DEFAULT_LEN     32
#define URI_LEN         64
#define NUMERIC_LEN     12
#define SENSOR_LEN      40
#define IP_LEN          16
#define FLOAT_LEN       16

#define FILE_LEN        0

static can5_cfg_meta_t default_elem[] = {
    CFG_ITEM(CFG_INIT,                  GENERAL_TAG,        TAG_FS,             BOOLEAN_LEN,    "true"          ),
    CFG_ITEM(CFG_OTA_MODE,              GENERAL_TAG,        NVS,                2,              "0"             ),

    CFG_ITEM(CFG_DEVICE_NAME,           GENERAL_TAG,        TAG_FS,             DEFAULT_LEN,    ""              ),
    CFG_ITEM(CFG_DEVICE_ORGANIZATION,   GENERAL_TAG,        TAG_FS,             DEFAULT_LEN,    "interlab"      ),
    CFG_ITEM(CFG_DEVICE_ID,             GENERAL_TAG,        RAM,                DEFAULT_LEN,    ""              ),
    CFG_ITEM(CFG_DEVICE_ID_POSTFIX,     GENERAL_TAG,        TAG_FS,             NUMERIC_LEN,    "1"             ),
    CFG_ITEM(CFG_PROJECT,               GENERAL_TAG,        TAG_FS,             URI_LEN,        "node/v5"       ),
    CFG_ITEM(CFG_APP_VERSION,           GENERAL_TAG,        RAM,                DEFAULT_LEN,    ""              ),

#if CONFIG_IDF_TARGET_ESP32
    CFG_ITEM(CFG_DEVICE_DATA_MODE,      GENERAL_TAG,        TAG_FS,             DEFAULT_LEN,    "CAN5_DATA_MODE_CYCLE"),
    CFG_ITEM(CFG_DATA_CYCLE_SEC,        GENERAL_TAG,        TAG_FS,             NUMERIC_LEN,    "120"           ),
#elif CONFIG_IDF_TARGET_ESP32S3
    CFG_ITEM(CFG_DEVICE_DATA_MODE,      GENERAL_TAG,        TAG_FS,             DEFAULT_LEN,    "CAN5_DATA_MODE_REALTIME"),
    CFG_ITEM(CFG_DATA_CYCLE_SEC,        GENERAL_TAG,        TAG_FS,             NUMERIC_LEN,    "1"           ),
#endif

    CFG_ITEM(CFG_HAZEMON_ENABLE,        COMMS_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),
    CFG_ITEM(CFG_HAZEMON_SYNC_INTERVAL, COMMS_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),

    CFG_ITEM(CFG_LORARELAY_ENABLE,      COMMS_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),
    CFG_ITEM(CFG_LORARELAY_GPS_CYCLE,   COMMS_TAG,          TAG_FS,             4,              "5"             ),

    CFG_ITEM(CFG_MQTT_DATA_ENABLE,      COMMS_TAG,          TAG_FS,             BOOLEAN_LEN,    "true"         ),
    CFG_ITEM(CFG_MQTT_CONFIGURATION_ENABLE,   COMMS_TAG,    TAG_FS,             BOOLEAN_LEN,    "true"         ),
    CFG_ITEM(CFG_MQTT_URI,              COMMS_TAG,          TAG_FS,             URI_LEN,        "mqtts://mqtt.hazemon.in.th"),
    CFG_ITEM(CFG_MQTT_PORT,             COMMS_TAG,          TAG_FS,             NUMERIC_LEN,    "8883"          ),
    CFG_ITEM(CFG_MQTT_USERNAME,         COMMS_TAG,          TAG_FS,             DEFAULT_LEN,    "hazemon-sensor"              ),
    CFG_ITEM(CFG_MQTT_PASSWORD,         COMMS_TAG,          TAG_FS,             DEFAULT_LEN,    "interlab12120/sensor"              ),
    CFG_ITEM(CFG_MQTT_ENCRYPTED,        COMMS_TAG,          TAG_FS,             BOOLEAN_LEN,    "true"         ),
    CFG_ITEM(CFG_MQTT_TLS_CERT,         MQTT_TLS_CERT,      RAW_FS,             FILE_LEN,       "-----BEGIN CERTIFICATE-----\n"
                                                                                                "MIIDrTCCApWgAwIBAgIUKyFf4I7aGqpwD6CsoIR+dfOf450wDQYJKoZIhvcNAQEL\n"
                                                                                                "BQAwZTELMAkGA1UEBhMCVEgxFDASBgNVBAgMC1BhdGh1bXRoYW5pMREwDwYDVQQK\n"
                                                                                                "DAhpbnRFUkxhYjEQMA4GA1UECwwHSGF6ZW1vbjEbMBkGA1UEAwwSbXF0dC5oYXpl\n"
                                                                                                "bW9uLmluLnRoMCAXDTIyMTIxMjE1MTQzNVoYDzQ3NjAxMTA3MTUxNDM1WjBlMQsw\n"
                                                                                                "CQYDVQQGEwJUSDEUMBIGA1UECAwLUGF0aHVtdGhhbmkxETAPBgNVBAoMCGludEVS\n"
                                                                                                "TGFiMRAwDgYDVQQLDAdIYXplbW9uMRswGQYDVQQDDBJtcXR0LmhhemVtb24uaW4u\n"
                                                                                                "dGgwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDQ+tSgo0OMjscsLc+z\n"
                                                                                                "iIvBKVQ2JswNbEgmSAEtON3yw6Pc9kjDCxtZ0B+EHJiSWbLcDBOqAjVgimGQgOpl\n"
                                                                                                "5wlWAqSOnwNCfLp5BdnPxnKnX3p6rSNe+AueASeVWkUFFqSxPkznXuwPL+E9cUgs\n"
                                                                                                "9RwOtj/6WmtLRG9WzL2KereInCKZ+qgkpC/muLRBRu3DeGn2Ln1P+PTuXUFD9naP\n"
                                                                                                "MG3QTcDR+IX1aKxSI7zzIwsN/5xXO3OfrwSTeW8efZbcY8pUDn+knm803VZeXrDi\n"
                                                                                                "oep0aO4rXZ1uqhmP3l4glpv6zT/9+HfsYiHKqpv3TzxzzV0hv5BZGP4xTTe9vk4u\n"
                                                                                                "8mOFAgMBAAGjUzBRMB0GA1UdDgQWBBSqMzM86iN9aF6K3txsb19ogEdKkzAfBgNV\n"
                                                                                                "HSMEGDAWgBSqMzM86iN9aF6K3txsb19ogEdKkzAPBgNVHRMBAf8EBTADAQH/MA0G\n"
                                                                                                "CSqGSIb3DQEBCwUAA4IBAQCE/xq/0LTFrm4+fTUQBUyU/vSBD4wOSfg7V96rG8+D\n"
                                                                                                "IfdLPv0yxZE9QU9QELlAr0lBgAqKj+wVjzGjb423298l7TBEmTvXXIZgtKGPymUo\n"
                                                                                                "seg+PLUKy7tO+8lSmv7w+41fQti8MzxXb5dVpnc3Ng35E2mJ4nDv9QEqxfJO7CWB\n"
                                                                                                "4N8U2v48qbz90OwPAgMI/eRjx0YlQHf1ZP1XjreXxn2/VnZGMeLl/MV7lBatOV+X\n"
                                                                                                "uBNN9qMS+z80GKlYztDx+iPwN48zsqvqrIux8D1ujBX957JvHoGdlqFpnC1w1Am2\n"
                                                                                                "tHISGxT+08pXsP6DRWMWjgvk84plY/rytuvxQhSB9knG\n"
                                                                                                "-----END CERTIFICATE-----"              ),
    CFG_ITEM(CFG_MQTT_WAIT_FOR_ACK,     COMMS_TAG,          TAG_FS,             BOOLEAN_LEN,    "true"         ),

    CFG_ITEM(CFG_WIFI_AP_ENABLE,        NETIF_TAG,          TAG_FS,             BOOLEAN_LEN,    "true"          ),
    CFG_ITEM(CFG_WIFI_AP_PASS,          NETIF_TAG,          TAG_FS,             DEFAULT_LEN,    "interlab"      ),

    CFG_ITEM(CFG_WIFI_STA_ENABLE,       NETIF_TAG,          TAG_FS,             BOOLEAN_LEN,    "true"         ),
    CFG_ITEM(CFG_WIFI_STA_SSID,         NETIF_TAG,          TAG_FS,             URI_LEN,        "Canarin"      ),
    CFG_ITEM(CFG_WIFI_STA_PASS,         NETIF_TAG,          TAG_FS,             DEFAULT_LEN,    "interlab"     ),

    CFG_ITEM(CFG_LWAN_ENABLE,           NETIF_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),

    CFG_ITEM(CFG_LWAN_OTAA,             NETIF_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),

    CFG_ITEM(CFG_LWAN_DEVEUI,           NETIF_TAG,          RAM,                24,             "00:00:00:00:00:00:00:00"),
    CFG_ITEM(CFG_LWAN_APPEUI,           NETIF_TAG,          TAG_FS,             24,             "99:88:77:66:99:88:77:66"),

    CFG_ITEM(CFG_LWAN_DADDR,            NETIF_TAG,          TAG_FS,             12,             "00:00:00:00"   ),
    CFG_ITEM(CFG_LWAN_RX_1_DELAY,       NETIF_TAG,          TAG_FS,             NUMERIC_LEN,    "5000"          ),
    CFG_ITEM(CFG_LWAN_RX_2_DELAY,       NETIF_TAG,          TAG_FS,             NUMERIC_LEN,    "6000"          ),

    CFG_ITEM(CFG_LWAN_ADAPTIVE_DATA_RATE, NETIF_TAG,        TAG_FS,             2,              "0"             ),
    CFG_ITEM(CFG_LWAN_DATA_RATE,        NETIF_TAG,          TAG_FS,             2,              "5"             ),
    CFG_ITEM(CFG_LWAN_TRANSMIT_POWER,   NETIF_TAG,          TAG_FS,             2,              "0"             ),
    CFG_ITEM(CFG_LWAN_USER_DATALEN,     NETIF_TAG,          TAG_FS,             4,              "0"             ),
    CFG_ITEM(CFG_LWAN_ADD_NUM_CYCLE_DATA, NETIF_TAG,        TAG_FS,             BOOLEAN_LEN,    "true"          ),

    CFG_ITEM(CFG_CELL_ENABLE,           NETIF_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),
    CFG_ITEM(CFG_CELL_APN,              NETIF_TAG,          TAG_FS,             16,             "internet"      ),
    CFG_ITEM(CFG_CELL_OPERATOR,         NETIF_TAG,          RAM,                DEFAULT_LEN,    ""              ),
    CFG_ITEM(CFG_CELL_ENABLE_GPS,       NETIF_TAG,          TAG_FS,             BOOLEAN_LEN,    "false"         ),

#if CONFIG_IDF_TARGET_ESP32
    CFG_ITEM(CFG_ADC_0,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_ADC_1,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_ADC_2,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_ADC_3,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_0,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_1,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_2,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_3,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_4,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_BME280"),
    CFG_ITEM(CFG_I2C_5,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_6,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_7,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_0,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_1,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_PM"),
    CFG_ITEM(CFG_UART_2,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_3,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_CO2"),
    CFG_ITEM(CFG_UART_4,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_5,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_6,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_GPS"),
    CFG_ITEM(CFG_UART_7,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_CO"),
#elif CONFIG_IDF_TARGET_ESP32S3
    CFG_ITEM(CFG_ADC_0,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_ADC_1,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_ADC_2,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_ADC_3,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_0,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_1,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_2,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_3,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_4,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_BME280"),
    CFG_ITEM(CFG_I2C_5,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_6,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_I2C_7,                 SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_0,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     "CAN5_SENSORDRIV_TYPE_PM"  ),
    CFG_ITEM(CFG_UART_1,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_2,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_3,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL),
    CFG_ITEM(CFG_UART_4,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_5,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL     ),
    CFG_ITEM(CFG_UART_6,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL),
    CFG_ITEM(CFG_UART_7,                SENSORS_TAG,        TAG_FS,             SENSOR_LEN,     SENSOR_NULL),
#endif

    CFG_ITEM(CFG_HIGH_FREQ_IMU,         SENSORS_TAG,        TAG_FS,             BOOLEAN_LEN,    "false"         ),
    CFG_ITEM(CFG_HIGH_FREQ_IMU_FREQ,    SENSORS_TAG,        TAG_FS,             NUMERIC_LEN,    "20"         ),

    CFG_ITEM(CFG_LOG_TO_SD,             LOG_TAG,            TAG_FS,             BOOLEAN_LEN,    "false"         ),
    CFG_ITEM(CFG_LOG_TO_NETSOCK,        LOG_TAG,            TAG_FS,             BOOLEAN_LEN,    "false"         ),
    CFG_ITEM(CFG_LOG_TO_NETSOCK_IP,     LOG_TAG,            TAG_FS,             IP_LEN,         "192.168.2.200" ),
    CFG_ITEM(CFG_LOG_TO_NETSOCK_PORT,   LOG_TAG,            TAG_FS,             NUMERIC_LEN,    "9745"          ),

    CFG_ITEM(CFG_WIFI_SNTP_SERVER,      GENERAL_TAG,        TAG_FS,             URI_LEN,        "pool.ntp.org"  ),
    CFG_ITEM(CFG_HAZEMON_IP,            COMMS_TAG,          TAG_FS,             IP_LEN,         "203.159.6.98"  ),
    CFG_ITEM(CFG_HAZEMON_PORT,          COMMS_TAG,          TAG_FS,             NUMERIC_LEN,    "60002"         ),

#if CONFIG_IDF_TARGET_ESP32
    CFG_ITEM(CFG_OTA_UPDATE_URL,        GENERAL_TAG,        TAG_FS,             URI_LEN,        "https://esp-ota.hazemon.in.th/can5/"),
#elif CONFIG_IDF_TARGET_ESP32S3
    CFG_ITEM(CFG_OTA_UPDATE_URL,        GENERAL_TAG,        TAG_FS,             URI_LEN,        "https://esp-ota.hazemon.in.th/can6/"),
#endif

    CFG_ITEM(CFG_LAST_G_LAT,            GPS_TAG,            TAG_FS,             FLOAT_LEN,      "14.0775217"    ),
    CFG_ITEM(CFG_LAST_G_LNG,            GPS_TAG,            TAG_FS,             FLOAT_LEN,      "100.61316"     ),
    CFG_ITEM(CFG_LAST_G_ALT,            GPS_TAG,            TAG_FS,             FLOAT_LEN,      "3.4"           ),
    CFG_ITEM(CFG_LAST_G_TIME,           GPS_TAG,            TAG_FS,             NUMERIC_LEN,    "0"             ),

    CFG_ITEM(CFG_LWAN_SEND_DELAY,       VAR_TAG,            RAM,                NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_LWAN_PKT_ID,           VAR_TAG,            TAG_FS,             NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_LWAN_FW_CTX0,          LORAWAN_CTX0,       RAW_FS,             FILE_LEN,       ""              ),
    CFG_ITEM(CFG_LWAN_FW_CTX1,          LORAWAN_CTX1,       RAW_FS,             FILE_LEN,       ""              ),
    CFG_ITEM(CFG_LWAN_FW_CTX2,          LORAWAN_CTX2,       RAW_FS,             FILE_LEN,       ""              ),
    CFG_ITEM(CFG_LWAN_FW_CTX3,          LORAWAN_CTX3,       RAW_FS,             FILE_LEN,       ""              ),
    CFG_ITEM(CFG_LWAN_FW_CTX4,          LORAWAN_CTX4,       RAW_FS,             FILE_LEN,       ""              ),
    CFG_ITEM(CFG_LWAN_FW_CTX5,          LORAWAN_CTX5,       RAW_FS,             FILE_LEN,       ""              ),
    CFG_ITEM(CFG_LWAN_FW_CTX6,          LORAWAN_CTX6,       RAW_FS,             FILE_LEN,       ""              ),

    CFG_ITEM(CFG_LWAN_STATS_SNR,        VAR_TAG,            RAM,                NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_LWAN_STATS_RSSI,       VAR_TAG,            RAM,                NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_LWAN_STATS_FCNT_UP,    VAR_TAG,            RAM,                NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_LWAN_STATS_FCNT_DOWN,  VAR_TAG,            RAM,                NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_LWAN_STATS_LAST_ACK,   VAR_TAG,            TAG_FS,             NUMERIC_LEN,    "0"             ),
    CFG_ITEM(CFG_MQTT_STATS_LAST_ACK,   VAR_TAG,            TAG_FS,             NUMERIC_LEN,    "0"             ),

    CFG_ITEM(CFG_CALIB_ZE07_CO_ADC_MULTI, CALIBRATION_TAG,  TAG_FS,             FLOAT_LEN,      "0.205"         ),
    CFG_ITEM(CFG_CALIB_ZE07_CO_ADC_BIAS, CALIBRATION_TAG,   TAG_FS,             FLOAT_LEN,      "-73.8"         ),
    CFG_ITEM(CFG_CALIB_ZE07_CO_BIAS, CALIBRATION_TAG,       TAG_FS,             FLOAT_LEN,      "-0.5"          ),


};

const can5_cfg_meta_t *can5_config_get_all_meta_elems(size_t *len)
{
    *len = sizeof(default_elem) / sizeof(default_elem[0]);
    return default_elem;
}

const can5_cfg_meta_tags_t *can5_get_cfg_tag_list(size_t *len)
{
    *len = sizeof(default_tags)/sizeof (default_tags[0]);
    return default_tags;
}

const char **can5_get_cfg_raw_files_list(size_t *len)
{
    *len = sizeof(raw_files_list) / sizeof (raw_files_list[0]);
    return raw_files_list;
}

const can5_cfg_meta_t *can5_config_get_meta(can5_cfg_type_t type)
{
     for (int i = 0; i < sizeof(default_elem)/ sizeof(default_elem[0]); i++) {
         if (default_elem[i].type == type) {
             return &default_elem[i];
         }
     }

     return NULL;
}


static const can5_tag_tab_t _can5_config_types_tags = {
    TAG_TAB_ITEM(CFG_INIT),
    TAG_TAB_ITEM(CFG_OTA_MODE),

    TAG_TAB_ITEM(CFG_DEVICE_NAME),
    TAG_TAB_ITEM(CFG_DEVICE_ORGANIZATION),
    TAG_TAB_ITEM(CFG_DEVICE_ID),
    TAG_TAB_ITEM(CFG_DEVICE_ID_POSTFIX),
    TAG_TAB_ITEM(CFG_PROJECT),
    TAG_TAB_ITEM(CFG_APP_VERSION),                   // in configuration mode, or running mode, etc.
    TAG_TAB_ITEM(CFG_DEVICE_DATA_MODE),
    TAG_TAB_ITEM(CFG_DATA_CYCLE_SEC),                // seconds separating two data collection cycle

    TAG_TAB_ITEM(CFG_HAZEMON_ENABLE),
    TAG_TAB_ITEM(CFG_HAZEMON_SYNC_INTERVAL),

    TAG_TAB_ITEM(CFG_LORARELAY_ENABLE),
    TAG_TAB_ITEM(CFG_LORARELAY_GPS_CYCLE),

    TAG_TAB_ITEM(CFG_MQTT_CONFIGURATION_ENABLE),
    TAG_TAB_ITEM(CFG_MQTT_DATA_ENABLE),
    TAG_TAB_ITEM(CFG_MQTT_URI),
    TAG_TAB_ITEM(CFG_MQTT_PORT),
    TAG_TAB_ITEM(CFG_MQTT_USERNAME),
    TAG_TAB_ITEM(CFG_MQTT_PASSWORD),
    TAG_TAB_ITEM(CFG_MQTT_WAIT_FOR_ACK),
    TAG_TAB_ITEM(CFG_MQTT_ENCRYPTED),
    TAG_TAB_ITEM(CFG_MQTT_TLS_CERT),

    TAG_TAB_ITEM(CFG_WIFI_AP_ENABLE),
    TAG_TAB_ITEM(CFG_WIFI_AP_PASS),

    TAG_TAB_ITEM(CFG_WIFI_STA_ENABLE),                 // ssid for wifi STA
    TAG_TAB_ITEM(CFG_WIFI_STA_SSID),                 // ssid for wifi STA
    TAG_TAB_ITEM(CFG_WIFI_STA_PASS),                 // password for wifi STA

    TAG_TAB_ITEM(CFG_LWAN_ENABLE),

    TAG_TAB_ITEM(CFG_LWAN_OTAA),

    TAG_TAB_ITEM(CFG_LWAN_DEVEUI),
    TAG_TAB_ITEM(CFG_LWAN_APPEUI),

    TAG_TAB_ITEM(CFG_LWAN_DADDR),
    TAG_TAB_ITEM(CFG_LWAN_RX_1_DELAY),
    TAG_TAB_ITEM(CFG_LWAN_RX_2_DELAY),

    TAG_TAB_ITEM(CFG_LWAN_ADAPTIVE_DATA_RATE),
    TAG_TAB_ITEM(CFG_LWAN_DATA_RATE),
    TAG_TAB_ITEM(CFG_LWAN_TRANSMIT_POWER),
    TAG_TAB_ITEM(CFG_LORARELAY_GPS_CYCLE),
    TAG_TAB_ITEM(CFG_LWAN_USER_DATALEN),
    TAG_TAB_ITEM(CFG_LWAN_ADD_NUM_CYCLE_DATA),

    TAG_TAB_ITEM(CFG_CELL_ENABLE),
    TAG_TAB_ITEM(CFG_CELL_APN),
    TAG_TAB_ITEM(CFG_CELL_OPERATOR),
    TAG_TAB_ITEM(CFG_CELL_ENABLE_GPS),

    TAG_TAB_ITEM(CFG_ADC_0),                         // sensor driver for the port
    TAG_TAB_ITEM(CFG_ADC_1),
    TAG_TAB_ITEM(CFG_ADC_2),
    TAG_TAB_ITEM(CFG_ADC_3),
    TAG_TAB_ITEM(CFG_I2C_0),
    TAG_TAB_ITEM(CFG_I2C_1),
    TAG_TAB_ITEM(CFG_I2C_2),
    TAG_TAB_ITEM(CFG_I2C_3),
    TAG_TAB_ITEM(CFG_I2C_4),
    TAG_TAB_ITEM(CFG_I2C_5),
    TAG_TAB_ITEM(CFG_I2C_6),
    TAG_TAB_ITEM(CFG_I2C_7),
    TAG_TAB_ITEM(CFG_UART_0),
    TAG_TAB_ITEM(CFG_UART_1),
    TAG_TAB_ITEM(CFG_UART_2),
    TAG_TAB_ITEM(CFG_UART_3),
    TAG_TAB_ITEM(CFG_UART_4),
    TAG_TAB_ITEM(CFG_UART_5),
    TAG_TAB_ITEM(CFG_UART_6),
    TAG_TAB_ITEM(CFG_UART_7),
    TAG_TAB_ITEM(CFG_HIGH_FREQ_IMU),
    TAG_TAB_ITEM(CFG_HIGH_FREQ_IMU_FREQ),

    TAG_TAB_ITEM(CFG_LOG_TO_SD),
    TAG_TAB_ITEM(CFG_LOG_TO_NETSOCK),                 // log to netsocket
    TAG_TAB_ITEM(CFG_LOG_TO_NETSOCK_IP),
    TAG_TAB_ITEM(CFG_LOG_TO_NETSOCK_PORT),

    TAG_TAB_ITEM(CFG_HAZEMON_IP),                    // ip address of hazemon server
    TAG_TAB_ITEM(CFG_HAZEMON_PORT),                  // UDP port to send sensor data to
    TAG_TAB_ITEM(CFG_WIFI_SNTP_SERVER),

    TAG_TAB_ITEM(CFG_OTA_UPDATE_URL),                // in configuration mode, or running mode, etc.


    TAG_TAB_ITEM(CFG_LAST_G_LAT),
    TAG_TAB_ITEM(CFG_LAST_G_LNG),
    TAG_TAB_ITEM(CFG_LAST_G_ALT),
    TAG_TAB_ITEM(CFG_LAST_G_TIME),


    TAG_TAB_ITEM(CFG_LWAN_SEND_DELAY),
    TAG_TAB_ITEM(CFG_LWAN_PKT_ID),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX0),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX1),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX2),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX3),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX4),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX5),
    TAG_TAB_ITEM(CFG_LWAN_FW_CTX6),

    TAG_TAB_ITEM(CFG_LWAN_STATS_SNR),
    TAG_TAB_ITEM(CFG_LWAN_STATS_RSSI),
    TAG_TAB_ITEM(CFG_LWAN_STATS_FCNT_UP),
    TAG_TAB_ITEM(CFG_LWAN_STATS_FCNT_DOWN),
    TAG_TAB_ITEM(CFG_LWAN_STATS_LAST_ACK),
    TAG_TAB_ITEM(CFG_MQTT_STATS_LAST_ACK),

    TAG_TAB_ITEM(CFG_CALIB_ZE07_CO_ADC_MULTI),
    TAG_TAB_ITEM(CFG_CALIB_ZE07_CO_ADC_BIAS),
    TAG_TAB_ITEM(CFG_CALIB_ZE07_CO_BIAS),
};

const char *can5_config_getstr(can5_cfg_type_t type)
{
    return TAG_LOOKUP(type, _can5_config_types_tags);
}

const char *can5_config_get_key_str(const can5_cfg_type_t type)
{
    return &TAG_LOOKUP(type, _can5_config_types_tags)[4];
}

static const can5_tag_tab_t _can5_config_storage_types_tags = {
        TAG_TAB_ITEM(CAN5_CFG_STORAGE_TYPE_TAG_FS),
        TAG_TAB_ITEM(CAN5_CFG_STORAGE_TYPE_RAW_FS),
        TAG_TAB_ITEM(CAN5_CFG_STORAGE_TYPE_RAM),
        TAG_TAB_ITEM(CAN5_CFG_STORAGE_TYPE_NVS),
};

const char* can5_config_storage_type_getstr(const can5_cfg_storage_type_t type)
{
    return TAG_LOOKUP(type, _can5_config_storage_types_tags);
}
