/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   28/02/22
*
* File:  can5_httprest_api.c
* Descr:
*******************************************************************************/

#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <can5_hal.h>
#include "can5_config.h"
#include "can5_utils.h"
#include "can5_storagedriv.h"
#include "esp_log.h"
#include "can5_http_json.h"
#include "can5_cron.h"
/*
 * For consistency all JSON values are string values.
 */
#if 1
static const char *TAG = "HTTP_JSON";
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

static cJSON *__get_json_from_params(can5_cfg_type_t *params, size_t params_len)
{
    TRACE_FUNC;

    cJSON *root;
    char *val;

    root = NULL;
    val = NULL;
    root = cJSON_CreateObject();

    if (!root) {
        goto error;
    }


    for (int i =0 ; i < params_len; i++) {
        if (config_manager.read(params[i], (uint8_t **)&val, NULL) != CAN5_SUCCESS) {
            goto error;
        }
        if (!strcmp(val, "")) {
            strcpy(val, "__NONE__");
        }
        if (!cJSON_AddStringToObject(root, can5_config_getstr(params[i]), val)) {
            goto error;
        }
        if (val) {
            free(val);
        }
        val = NULL;
    }

done:
    if (val) {
        free (val);
    }
    return root;

error:
    cJSON_Delete(root);
    root = NULL;
    goto done;
}

static char *__get_json_general()
{
    TRACE_FUNC;
    cJSON *root;
    char *response;

    can5_cfg_type_t params[] = {
        CFG_DEVICE_NAME,
        CFG_DEVICE_ORGANIZATION,
        CFG_DEVICE_ID,
        CFG_DEVICE_ID_POSTFIX,
        CFG_PROJECT,
        CFG_APP_VERSION,
        CFG_DEVICE_DATA_MODE,
        CFG_DATA_CYCLE_SEC,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

static char *__get_json_communication()
{
    TRACE_FUNC;
    cJSON *root;
    char *response;

    can5_cfg_type_t params[] = {
        CFG_MQTT_CONFIGURATION_ENABLE,
        CFG_MQTT_DATA_ENABLE,
        CFG_MQTT_URI,
        CFG_MQTT_PORT,
        CFG_MQTT_USERNAME,
        CFG_MQTT_PASSWORD,
        CFG_MQTT_WAIT_FOR_ACK,
        CFG_MQTT_ENCRYPTED,
        CFG_MQTT_TLS_CERT,

        CFG_HAZEMON_ENABLE,
        CFG_HAZEMON_SYNC_INTERVAL,

        CFG_LORARELAY_ENABLE,
        CFG_LORARELAY_GPS_CYCLE,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

char *__get_formatted_mac_address(const uint8_t * mac)
{
    static char formatted_mac[18] = { '\0' };
    char mac_str[13] = { '\0' };

    int i, j;

    can5_bin_to_hex(mac, mac_str, 6);

    i = j = 0;

    while(i < 12) {
        if (i % 2 == 0 && i != 0) {
            formatted_mac[j++] = ':';
        }
        formatted_mac[j++] = mac_str[i++];
    }

    return formatted_mac;
}

static char *__get_json_wifi_ap()
{
    TRACE_FUNC;
    cJSON *root, *elem;
    char *response;

    uint8_t ap_mac[6];
    char *ap_mac_str;

    can5_cfg_type_t params[] = {
        CFG_WIFI_AP_ENABLE,
        CFG_DEVICE_NAME,
        CFG_WIFI_AP_PASS,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

#ifndef CAN5_LINUX_HOST_TEST
    hal.get_mac_addresses(ap_mac, NULL);

    ap_mac_str = __get_formatted_mac_address(ap_mac);

    elem = cJSON_AddStringToObject(root, "AP_MAC", ap_mac_str);
    if (!elem) {
        goto done;
    }
#endif

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

static char *__get_json_wifi_sta()
{
    TRACE_FUNC;
    cJSON *root, *elem;
    char *response;

    uint8_t sta_mac[6];
    char *sta_mac_str;

    can5_cfg_type_t params[] = {
        CFG_WIFI_STA_ENABLE,
        CFG_WIFI_STA_SSID,
        CFG_WIFI_STA_PASS,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));

    if (!root) {
        goto done;
    }


#ifndef CAN5_LINUX_HOST_TEST
    hal.get_mac_addresses(NULL, (uint8_t *)sta_mac);
    sta_mac_str = __get_formatted_mac_address(sta_mac);

    elem = cJSON_AddStringToObject(root, "STA_MAC", sta_mac_str);
    if (!elem) {
        goto done;
    }

    elem = cJSON_AddStringToObject(root, "STA_IP", hal.get_ip_sta());
    if (!elem) {
        goto done;
    }
#endif

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

static char *__get_json_cellular()
{
    TRACE_FUNC;
    cJSON *root, *elem;
    char *response;

    can5_cfg_type_t params[] = {
        CFG_CELL_ENABLE,
        CFG_CELL_APN,
        CFG_CELL_OPERATOR,
        CFG_CELL_ENABLE_GPS,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

#ifndef CAN5_LINUX_HOST_TEST
    elem = cJSON_AddStringToObject(root, "CELL_IP", hal.get_ip_cell());
    if (!elem) {
        goto done;
    }
#endif
    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

static char *__get_json_lorawan()
{
    TRACE_FUNC;
    cJSON *root;
    char *response;

    can5_cfg_type_t params[] = {
        CFG_LWAN_ENABLE,
        CFG_LWAN_OTAA,
        CFG_LWAN_DEVEUI,
        CFG_LWAN_APPEUI,
        CFG_LWAN_DADDR,
        CFG_LWAN_RX_1_DELAY,
        CFG_LWAN_RX_2_DELAY,
        CFG_LWAN_ADAPTIVE_DATA_RATE,
        CFG_LWAN_DATA_RATE,
        CFG_LWAN_TRANSMIT_POWER,
        CFG_LORARELAY_GPS_CYCLE,
        CFG_LWAN_USER_DATALEN,
        CFG_LWAN_ADD_NUM_CYCLE_DATA,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

static char *__get_json_io()
{
    TRACE_FUNC;
    cJSON *root;
    char *response;

    can5_cfg_type_t params[] = {
        CFG_ADC_0,
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
        CFG_CALIB_ZE07_CO_BIAS,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

static char *__get_json_crontab()
{
    TRACE_FUNC;
    cJSON *root;
    char *response, *crontab;

    crontab = NULL;
    root = NULL;
    response = NULL;

    if (can5_read_crontab(&crontab) != CAN5_SUCCESS) {
        goto done;
    }

    root = cJSON_CreateObject();
    if (!root) {
        goto done;
    }

    if (!cJSON_AddStringToObject(root, "crontab", crontab)) {
        goto done;
    }

    response = cJSON_Print(root);

done:
    if (root) {
        cJSON_Delete(root);
    }

    if (crontab) {
        free(crontab);
    }

    return response;
}

static char *__get_json_logging()
{
    TRACE_FUNC;
    cJSON *root;
    char *response;

    can5_cfg_type_t params[] = {
        CFG_LOG_TO_SD,
        CFG_LOG_TO_NETSOCK,
        CFG_LOG_TO_NETSOCK_IP,
        CFG_LOG_TO_NETSOCK_PORT,
    };

    response = NULL;
    root = NULL;

    root = __get_json_from_params(params, sizeof(params)/sizeof (params[0]));
    if (!root) {
        goto done;
    }

    response = cJSON_Print(root);

done:
    cJSON_Delete(root);
    return response;
}

char *can5_http_json_str_get(can5_http_json_type_t type)
{
    TRACE_FUNC;

    switch (type) {

        case CAN5_HTTP_JSON_GENERAL:
            return __get_json_general();

        case CAN5_HTTP_JSON_COMMUNICATION:
            return __get_json_communication();

        case CAN5_HTTP_JSON_WIFI_AP:
            return __get_json_wifi_ap();

        case CAN5_HTTP_JSON_WIFI_STA:
            return __get_json_wifi_sta();

        case CAN5_HTTP_JSON_CELLULAR:
            return __get_json_cellular();

        case CAN5_HTTP_JSON_LORAWAN:
            return __get_json_lorawan();

        case CAN5_HTTP_JSON_IO:
            return __get_json_io();

        case CAN5_HTTP_JSON_CRONTAB:
            return __get_json_crontab();

        case CAN5_HTTP_JSON_LOGGING:
            return __get_json_logging();

        default:
            return NULL;
    }
}

static bool __validate_whole_num(const char *param)
{
    const char *p;
    p = param;

    while (*p != '\0') {
        if (!isdigit((int)*p)) {
            return false;
        }
        p++;
    }
    return true;
}

static bool __validate_hexchar(const char *param)
{
    const char *p;
    p = param;
    size_t pos = 1;

    while (*p != '\0') {
        if (*p == ':' && pos % 3 == 0) {
            // pass through
            {};
        }
        else if (!isxdigit((int)*p)) {
            return false;
        }
        pos++;
        p++;
    }
    return true;
}

__attribute__((unused)) static bool __validate_max_len(const char *param, size_t len)
{
    return strlen(param) <= len;
}

static bool __validate_min_len(const char *param, size_t len)
{
    return strlen(param) >= len;
}

static bool __validate_fixed_len(const char *param, size_t len)
{
    return strlen(param) == len;
}


static bool __validate_boolean(const char *param)
{
    return strcmp(CAN5_TRUE_TOKEN, param) == 0 || strcmp(CAN5_FALSE_TOKEN, param) == 0;
}

static bool __validate_comm_method(can5_cfg_type_t type, const char *param)
{
    bool wifi_available, cellular_available, inet_available, lorawan_available, is_true;

    config_manager.read_bool(CFG_WIFI_STA_ENABLE, &wifi_available);
    config_manager.read_bool(CFG_CELL_ENABLE, &cellular_available);

    inet_available = wifi_available | cellular_available;
    config_manager.read_bool(CFG_LWAN_ENABLE, &lorawan_available);

    is_true = strcmp(CAN5_TRUE_TOKEN, param) == 0;

    if (is_true) {
        switch (type) {
            case CFG_MQTT_CONFIGURATION_ENABLE:
            case CFG_MQTT_DATA_ENABLE:
            case CFG_HAZEMON_ENABLE:
                return inet_available;

            case CFG_LORARELAY_ENABLE:
                return lorawan_available;
            default:
                return false;
        }
    }

    return true;
}

static bool __validate_netif_external(can5_cfg_type_t type, const char *param)
{
    bool cellular_available, lorawan_available, is_true;

    config_manager.read_bool(CFG_CELL_ENABLE, &cellular_available);
    config_manager.read_bool(CFG_LWAN_ENABLE, &lorawan_available);

    is_true = strcmp(CAN5_TRUE_TOKEN, param) == 0;

    if (is_true) {
        switch (type) {
            case CFG_CELL_ENABLE:
                // check if lorawan is enabled
                return !lorawan_available;

            case CFG_LWAN_ENABLE:
                // check if cell is enabled
                return !cellular_available;

            default:
                return false;
        }
    }

    return true;
}

bool can5_validate_param(can5_cfg_type_t type, const char *param, bool *skip)
{
    TRACE_FUNC;
    bool result = true;
    *skip = false;
    // if __NONE__ then verification is complete
    if (!strcmp(param, "__NONE__")) {
        return true;
    }

    switch (type) {

        /* General */
        case CFG_DEVICE_NAME:
            result = __validate_min_len(param, 1);
            break;
        case CFG_DEVICE_ORGANIZATION:
            result = __validate_min_len(param, 1);
            break;
        case CFG_DEVICE_ID:
            *skip = true;
            break;
        case CFG_DEVICE_ID_POSTFIX:
            result = __validate_min_len(param, 1);
            break;
        case CFG_PROJECT:
            result = __validate_min_len(param, 1);
            break;
        case CFG_APP_VERSION:
            *skip = true;
            break;
        case CFG_DEVICE_DATA_MODE:
            break;
        case CFG_DATA_CYCLE_SEC:
            result = __validate_whole_num(param) && __validate_min_len(param, 1);
            break;

        /* Communication */
        case CFG_MQTT_DATA_ENABLE:                   // log to mqtt
        case CFG_MQTT_CONFIGURATION_ENABLE:
            result = __validate_boolean(param) && __validate_comm_method(type, param);
            break;
        case CFG_MQTT_URI:
            result = __validate_min_len(param, 3);
            break;
        case CFG_MQTT_PORT:
            result = __validate_whole_num(param);
            break;
        case CFG_MQTT_USERNAME:
            break;
        case CFG_MQTT_PASSWORD:
            break;
        case CFG_MQTT_WAIT_FOR_ACK:
            result = __validate_boolean(param);
            break;
        case CFG_MQTT_ENCRYPTED:
            result = __validate_boolean(param);
            break;
        case CFG_MQTT_TLS_CERT:
            break;
        case CFG_HAZEMON_ENABLE:
            result = __validate_boolean(param) && __validate_comm_method(type, param);
            break;
        case CFG_HAZEMON_SYNC_INTERVAL:
            result = __validate_boolean(param);
            break;

        case CFG_LORARELAY_ENABLE:
            result = __validate_boolean(param) && __validate_comm_method(type, param);
            break;
        case CFG_LORARELAY_GPS_CYCLE:
            result = __validate_whole_num(param);
            break;


        /* Wifi AP */
        case CFG_WIFI_AP_ENABLE:
            result = __validate_boolean(param);
            break;
        case CFG_WIFI_AP_PASS:
            result = __validate_min_len(param, 8);
            break;

        /* Wifi STA */
        case CFG_WIFI_STA_ENABLE:
            result = __validate_boolean(param);
            break;
        case CFG_WIFI_STA_SSID:
            result = __validate_min_len(param, 2);
            break;
        case CFG_WIFI_STA_PASS:
            // it's either empty or minimum 8
            result = __validate_fixed_len(param, 0) || __validate_min_len(param, 8);
            break;

        case CFG_CELL_ENABLE:
            result = __validate_boolean(param) && __validate_netif_external(type, param);
            break;
        case CFG_CELL_APN:
            break;
        case CFG_CELL_ENABLE_GPS:
            result = __validate_boolean(param);
            break;

        /* LWAN */
        case CFG_LWAN_ENABLE:
            result = __validate_boolean(param) && __validate_netif_external(type, param);
            break;
        case CFG_LWAN_OTAA:
            result = __validate_boolean(param);
            break;
        case CFG_LWAN_DEVEUI:
            *skip = true;
            break;
        case CFG_LWAN_APPEUI:
            result = __validate_hexchar(param) && __validate_fixed_len(param, 23);
            break;
        case CFG_LWAN_DADDR:
            result = __validate_hexchar(param) && __validate_fixed_len(param, 11);
            break;
        case CFG_LWAN_RX_1_DELAY:
            result = __validate_whole_num(param);
            break;
        case CFG_LWAN_RX_2_DELAY:
            result = __validate_whole_num(param);
            break;
        case CFG_LWAN_ADAPTIVE_DATA_RATE:
            result = __validate_whole_num(param);
            break;
        case CFG_LWAN_DATA_RATE:
            result = __validate_whole_num(param);
            break;
        case CFG_LWAN_TRANSMIT_POWER:
            result = __validate_whole_num(param);
            break;
        case CFG_LWAN_USER_DATALEN:
            result = __validate_whole_num(param);
            break;
        case CFG_LWAN_ADD_NUM_CYCLE_DATA:
            result = __validate_boolean(param);
            break;

        /* Sensors */
        case CFG_ADC_0:
            break;
        case CFG_ADC_1:
            break;
        case CFG_ADC_2:
            break;
        case CFG_ADC_3:
            break;
        case CFG_I2C_0:
            break;
        case CFG_I2C_1:
            break;
        case CFG_I2C_2:
            break;
        case CFG_I2C_3:
            break;
        case CFG_I2C_4:
            break;
        case CFG_I2C_5:
            break;
        case CFG_I2C_6:
            break;
        case CFG_I2C_7:
            break;
        case CFG_UART_0:
            break;
        case CFG_UART_1:
            break;
        case CFG_UART_2:
            break;
        case CFG_UART_3:
            break;
        case CFG_UART_4:
            break;
        case CFG_UART_5:
            break;
        case CFG_UART_6:
            break;
        case CFG_UART_7:
            break;
        case CFG_HIGH_FREQ_IMU:
            result = __validate_boolean(param);
            break;
        case CFG_HIGH_FREQ_IMU_FREQ:
            result = __validate_whole_num(param);
            break;
        case CFG_CALIB_ZE07_CO_BIAS:
            result = __validate_min_len(param, 1);
            break;

        case CFG_LOG_TO_SD:
            result = __validate_boolean(param);
            break;
        case CFG_LOG_TO_NETSOCK:
            result = __validate_boolean(param);
            break;
        case CFG_LOG_TO_NETSOCK_IP:
            break;
        case CFG_LOG_TO_NETSOCK_PORT:
            result = __validate_whole_num(param);
            break;

        case CFG_LAST_G_LNG:
        case CFG_LAST_G_LAT:
        case CFG_LAST_G_ALT:
        case CFG_LAST_G_TIME:
            // hack to accept all data types.
            result = true;
            break;
        default:
            *skip = true;
            result = false;
            break;
    }

    return result;
}

static const char *__none__ = "";

static char * __set_params_from_json(can5_cfg_type_t *params, size_t param_len, const char *body)
{
    TRACE_FUNC;

    cJSON *root, *res_root;
    bool skip;

    char *response;

    response = NULL;
    root = res_root = NULL;

    TRACE_FUNC;
    root = cJSON_Parse(body);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        goto done;
    }

    TRACE_FUNC;
    res_root = cJSON_CreateObject();

    if (!res_root) {
        goto done;
    }

    TRACE_FUNC;
    for (int i = 0; i < param_len; i ++) {
        const char *key = can5_config_getstr(params[i]);
        const char *val;
        char err_key[CAN5_STORAGE_KEY_LEN_MAX + 8];
        CLEAR_ARRAY(err_key);
        snprintf(err_key, CAN5_STORAGE_KEY_LEN_MAX + 8, "%s_STATUS", key);

        if (cJSON_HasObjectItem(root, key)) {
            cJSON *elem;
            char *elem_str;
            if ((elem = cJSON_GetObjectItem(root, key)) == NULL) {
                response = NULL;
                goto done;
            }

            if ((elem_str = cJSON_GetStringValue(elem)) == NULL) {
                cJSON_Delete(elem);
                response = NULL;
                goto done;
            }

            bool failed = false;
            if (can5_validate_param(params[i], elem_str, &skip)) {
                if (!strcmp(elem_str, "__NONE__")) {
                    val = __none__;
                }
                else {
                    val = elem_str;
                }

                if (!skip) {
                    if (config_manager.write(params[i], (uint8_t *)val, strlen(val)) != CAN5_SUCCESS) {
                        // write failed
                        failed = true;
                        if (!cJSON_AddStringToObject(res_root, err_key, "SAVE_ERROR")) {
                            response = NULL;
                            goto done;
                        }
                    }
                }

            }
            else {
                // validation failed
                failed = true;
                if (!cJSON_AddStringToObject(res_root, err_key, "PARAM_INVALID")) {
                    response = NULL;
                    goto done;
                }

            }

            if (!failed) {
                if (!cJSON_AddStringToObject(res_root, err_key, "SUCCESS")) {
                    response = NULL;
                    failed = true;
                    goto done;
                }
            }
            // add to json
            if (!cJSON_AddStringToObject(res_root, key,  elem_str)) {
                response = NULL;
                goto done;
            }
        }
    }

    response = cJSON_Print(res_root);
    TRACE_FUNC;

done:
    TRACE_FUNC;
    cJSON_Delete(root);
    cJSON_Delete(res_root);
    return response;
}

static char * __set_json_general(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_DEVICE_NAME,
        CFG_DEVICE_ORGANIZATION,
        CFG_DEVICE_ID,
        CFG_DEVICE_ID_POSTFIX,
        CFG_PROJECT,
        CFG_APP_VERSION,
        CFG_DEVICE_DATA_MODE,
        CFG_DATA_CYCLE_SEC,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_communication(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_MQTT_CONFIGURATION_ENABLE,
        CFG_MQTT_DATA_ENABLE,
        CFG_MQTT_URI,
        CFG_MQTT_PORT,
        CFG_MQTT_USERNAME,
        CFG_MQTT_PASSWORD,
        CFG_MQTT_WAIT_FOR_ACK,
        CFG_MQTT_ENCRYPTED,
        CFG_MQTT_TLS_CERT,

        CFG_HAZEMON_ENABLE,
        CFG_HAZEMON_SYNC_INTERVAL,

        CFG_LORARELAY_ENABLE,
        CFG_LORARELAY_GPS_CYCLE,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_wifi_ap(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_WIFI_AP_ENABLE,
        CFG_WIFI_AP_PASS,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_wifi_sta(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_WIFI_STA_ENABLE,
        CFG_WIFI_STA_SSID,
        CFG_WIFI_STA_PASS,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_cellular(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_CELL_ENABLE,
        CFG_CELL_APN,
        CFG_CELL_ENABLE_GPS,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_lorawan(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_LWAN_ENABLE,
        CFG_LWAN_OTAA,
        CFG_LWAN_DEVEUI,
        CFG_LWAN_APPEUI,
        CFG_LWAN_DADDR,
        CFG_LWAN_RX_1_DELAY,
        CFG_LWAN_RX_2_DELAY,
        CFG_LWAN_ADAPTIVE_DATA_RATE,
        CFG_LWAN_DATA_RATE,
        CFG_LWAN_TRANSMIT_POWER,
        CFG_LORARELAY_GPS_CYCLE,
        CFG_LWAN_USER_DATALEN,
        CFG_LWAN_ADD_NUM_CYCLE_DATA
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_io(const char *body)
{
    TRACE_FUNC;

    can5_cfg_type_t params[] = {
        CFG_ADC_0,
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
        CFG_CALIB_ZE07_CO_BIAS,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

static char * __set_json_crontab(const char *body)
{
    TRACE_FUNC;

    cJSON *root, *res_root;

    char *response;

    response = NULL;
    root = res_root = NULL;

    root = cJSON_Parse(body);
    if (!root) {
        goto done;
    }

    res_root = cJSON_CreateObject();

    if (!res_root) {
        goto done;
    }

    cJSON *crontab = cJSON_GetObjectItem(root, "crontab");
    if (!crontab) {
        goto done;
    }

    char *str = cJSON_GetStringValue(crontab);

    if (can5_write_crontab(str) != CAN5_SUCCESS) {
        cJSON_AddStringToObject(res_root, "status", "error writing crontab.");
        goto done;
    }

    cJSON_AddStringToObject(res_root, "crontab", str);

    response = cJSON_Print(res_root);

done:
    cJSON_Delete(root);
    cJSON_Delete(res_root);
    return response;
}

static char * __set_json_logging(const char *body)
{
    TRACE_FUNC;
    can5_cfg_type_t params[] = {
        CFG_LOG_TO_SD,
        CFG_LOG_TO_NETSOCK,
        CFG_LOG_TO_NETSOCK_IP,
        CFG_LOG_TO_NETSOCK_PORT,
        CFG_LWAN_DEVEUI,
        CFG_LWAN_APPEUI,
    };

    return __set_params_from_json(params, sizeof(params) / sizeof(params[0]), body);
}

char * can5_http_json_set(can5_http_json_type_t type, const char *body)
{
    TRACE_FUNC;

    switch (type) {

        case CAN5_HTTP_JSON_GENERAL:
            return __set_json_general(body);

        case CAN5_HTTP_JSON_COMMUNICATION:
            return __set_json_communication(body);

        case CAN5_HTTP_JSON_WIFI_AP:
            return __set_json_wifi_ap(body);

        case CAN5_HTTP_JSON_WIFI_STA:
            return __set_json_wifi_sta(body);

        case CAN5_HTTP_JSON_CELLULAR:
            return __set_json_cellular(body);

        case CAN5_HTTP_JSON_LORAWAN:
            return __set_json_lorawan(body);

        case CAN5_HTTP_JSON_IO:
            return __set_json_io(body);

        case CAN5_HTTP_JSON_CRONTAB:
            return __set_json_crontab(body);

        case CAN5_HTTP_JSON_LOGGING:
            return __set_json_logging(body);

        default:
            return NULL;
    }
}

