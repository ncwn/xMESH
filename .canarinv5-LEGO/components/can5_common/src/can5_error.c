#include <esp_system.h>
#include "can5_error.h"
#include "esp_log.h"
#include "can5_utils.h"

#if CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/ets_sys.h"
#endif

static can5_tag_tab_t _can5_err_tab = {
    TAG_TAB_ITEM(CAN5_ERR_GENERIC             ),
    TAG_TAB_ITEM(CAN5_ERR_NULL                ),
    TAG_TAB_ITEM(CAN5_ERR_INVALID_STATE       ),
    TAG_TAB_ITEM(CAN5_ERR_FREERTOS_pdFAIL     ),
    TAG_TAB_ITEM(CAN5_ERR_OUT_OF_HEAP_MEMORY  ),
    TAG_TAB_ITEM(CAN5_ERR_INVALID_DRIVERCTL   ),
    TAG_TAB_ITEM(CAN5_ERR_INVALID_PARAM       ),
    TAG_TAB_ITEM(CAN5_ERR_MEMCPY              ),
    TAG_TAB_ITEM(CAN5_ERR_UNIMPLEMENTED       ),
    TAG_TAB_ITEM(CAN5_ERR_REGEX               ),

    TAG_TAB_ITEM(CAN5_NET_ERR_BASE            ),
    TAG_TAB_ITEM(CAN5_NET_ERR_NOIF            ),
    TAG_TAB_ITEM(CAN5_NET_ERR_NIC_TIMEOUT     ),
    TAG_TAB_ITEM(CAN5_NET_ERR_CONN_TIMEOUT    ),
    TAG_TAB_ITEM(CAN5_NET_ERR_INVALID_APP_PORT),
    TAG_TAB_ITEM(CAN5_NET_ERR_BUSY            ),
    TAG_TAB_ITEM(CAN5_NET_LWAN_RESETTING_FCNT ),
    TAG_TAB_ITEM(CAN5_NET_ERR_INVALID_DRIVER  ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_INVALID_WIFI_MODE),
    TAG_TAB_ITEM(CAN5_NET_ERR_PARSE_INCOMPLETE),
    TAG_TAB_ITEM(CAN5_NET_ERR_DISCONNECTEDIF  ),

    TAG_TAB_ITEM(CAN5_HAL_ERR_BUSY            ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_TIMEOUT         ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_PORT_DISABLED   ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_INVALID_PORT    ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_NO_PATTERN      ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_SERIAL_RECV     ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_SERIAL_SEND     ),
    TAG_TAB_ITEM(CAN5_HAL_ERR_PATTERN_ENABLED ),

    TAG_TAB_ITEM(CAN5_CFG_ERR_GET             ),
    TAG_TAB_ITEM(CAN5_CFG_ERR_SET             ),
    TAG_TAB_ITEM(CAN5_CFG_ERR_INVALID_VALUE   ),
    TAG_TAB_ITEM(CAN5_CFG_ERR_JSON            ),
    TAG_TAB_ITEM(CAN5_CFG_ERR_REQ_CMD_INVALID ),

    TAG_TAB_ITEM(CAN5_STORAGE_ERR_BUSY        ),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_EMPTY       ),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_INVALID_INDEX),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_KEY_NOT_FOUND),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_KEY_TOO_LONG),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_TAG_NOT_FOUND),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_TAG_TOO_LONG),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_FILE_NOT_FOUND),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_FILESYSTEM  ),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_JSON        ),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_NO_SPACE    ),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_SEARCH_FAIL ),
    TAG_TAB_ITEM(CAN5_STORAGE_ERR_DIR_NOT_EMPTY),

    TAG_TAB_ITEM(CAN5_TIME_ERR_MKTIME         ),
    TAG_TAB_ITEM(CAN5_TIME_ERR_SET_SYS_TIME   ),
    TAG_TAB_ITEM(CAN5_TIME_ERR_GET_SYS_TIME   ),
    TAG_TAB_ITEM(CAN5_TIME_ERR_SET_RTC_TIME   ),

    TAG_TAB_ITEM(CAN5_SENSOR_ERR_PARSE_INCOMPLETE ),
    TAG_TAB_ITEM(CAN5_SENSOR_ERR_PARSE_ERROR  ),
    TAG_TAB_ITEM(CAN5_SENSOR_ERR_INVALID_PORT ),
    TAG_TAB_ITEM(CAN5_SENSOR_ERR_BUSY         ),

    TAG_TAB_ITEM(CAN5_NMEA_PARSE_ERROR        ),

    TAG_TAB_ITEM(CAN5_SENSOR_ERR_TIMEOUT      ),

    TAG_TAB_ITEM(CAN5_CODEC_ERR_PARTITION_NOT_FOUND),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_NO_DATAPOINT  ),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_INVALID_TOKEN ),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_INVALID_DATATYPE),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_VAL_PARSE     ),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_NO_TIMESTAMP  ),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_INVALID_CODEC ),
    TAG_TAB_ITEM(CAN5_CODEC_ERR_RX_PARSE      ),

    TAG_TAB_ITEM(CAN5_HTTP_ERR_AUTH           ),

    TAG_TAB_ITEM(CAN5_MISC_ERR_OTA_FAILED     ),

    TAG_TAB_ITEM(CAN5_SENSOR_ERR_BME280_DETECT),

    TAG_TAB_ITEM(CAN5_ERR_LAST                ),
};

bool _can5_error_check(can5_err_t code, const char* function, const char* file,  int line, bool abort) {
    if (code == ESP_OK) return false;
    if ((code >= CAN5_ERR_BASE ) && code < CAN5_ERR_LAST) {
        ESP_LOGE("CAN5_ERROR","0x%X [%s] in func. %s (%s:%d): ", code, TAG_LOOKUP(code, _can5_err_tab), function, file, line);
        if (abort) {
            goto abort;
        }
        return true;
    }

    ESP_LOGE("ESP_ERROR","0x%X [%s] in func. %s (%s:%d): ", code, esp_err_to_name(code), function, file, line);
    if (abort) {
        goto abort;
    }
    return true;
abort:
    can5_restart();
    esp_system_abort("CAN5 error check failed.");
    return false;
}

const char *can5_err_to_str(can5_err_t code)
{
    if (code == ESP_OK || code == CAN5_SUCCESS) return "OK";

    if ((code >= CAN5_ERR_BASE ) && code < CAN5_ERR_LAST) {
        return TAG_LOOKUP(code, _can5_err_tab);
    }

    return esp_err_to_name(code);
}
