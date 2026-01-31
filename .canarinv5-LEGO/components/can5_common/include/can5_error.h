/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can5_module.h
 * Descr: Interface class whiche specify the prototype of a module
 *        Every module is considered to be an indipended state machine 
 *        initialized with init() and runned by calling the process() method
 *******************************************************************************/

#ifndef __CAN5_ERR_H__
#define __CAN5_ERR_H__

#include <stdint.h>
#include "esp_err.h"
#include "stdbool.h"

#define CAN5_SUCCESS ESP_OK
#define CAN5_ERROR   ESP_FAIL

#define CAN5_ERR_BASE  0x00010000

typedef esp_err_t can5_err_t;

enum can5_err_list_e {
    CAN5_ERR_GENERIC               = CAN5_ERR_BASE,
    CAN5_ERR_NULL                  ,
    CAN5_ERR_INVALID_STATE         ,
    CAN5_ERR_FREERTOS_pdFAIL       ,
    CAN5_ERR_OUT_OF_HEAP_MEMORY    ,
    CAN5_ERR_INVALID_DRIVERCTL     ,
    CAN5_ERR_INVALID_PARAM         ,
    CAN5_ERR_MEMCPY                ,
    CAN5_ERR_UNIMPLEMENTED         ,
    CAN5_ERR_REGEX                 ,

    CAN5_NET_ERR_BASE              ,
    CAN5_NET_ERR_NOIF              ,
    CAN5_NET_ERR_NIC_TIMEOUT       ,
    CAN5_NET_ERR_CONN_TIMEOUT      ,
    CAN5_NET_ERR_INVALID_APP_PORT  ,
    CAN5_NET_ERR_BUSY              ,
    CAN5_NET_LWAN_RESETTING_FCNT   ,
    CAN5_NET_ERR_INVALID_DRIVER    ,
    CAN5_HAL_ERR_INVALID_WIFI_MODE ,
    CAN5_NET_ERR_PARSE_INCOMPLETE  ,
    CAN5_NET_ERR_DISCONNECTEDIF    ,     /**< underlying interface is disconnected */

    /** Hal Errors */
    CAN5_HAL_ERR_BUSY              ,     /**< resources is busy and cannot be acquired */
    CAN5_HAL_ERR_TIMEOUT           ,     /**< operation timeout */
    CAN5_HAL_ERR_SERIAL_RECV       ,     /**< rserial receive error */
    CAN5_HAL_ERR_SERIAL_SEND       ,     /**< serial send error */
    CAN5_HAL_ERR_PORT_DISABLED     ,     /**< port disabled */
    CAN5_HAL_ERR_INVALID_PORT      ,     /**< port out of range */
    CAN5_HAL_ERR_NO_PATTERN        ,     /**< pattern enabled and no detection yet */
    CAN5_HAL_ERR_PATTERN_ENABLED   ,     /**< pattern enabled, therefore cannot use normal read */

    /** Config Errors */
    CAN5_CFG_ERR_GET               ,     /**< Get config can't execute */
    CAN5_CFG_ERR_SET               ,     /**< Set config can't execute */
    CAN5_CFG_ERR_INVALID_VALUE     ,     /**< Invalid value execute */
    CAN5_CFG_ERR_JSON              ,     /**< Wrong json format */
    CAN5_CFG_ERR_REQ_CMD_INVALID   ,     /**< Request cmd invalid */

    CAN5_STORAGE_ERR_BUSY          ,
    CAN5_STORAGE_ERR_EMPTY         ,
    CAN5_STORAGE_ERR_INVALID_INDEX ,
    CAN5_STORAGE_ERR_KEY_NOT_FOUND ,
    CAN5_STORAGE_ERR_KEY_TOO_LONG  ,
    CAN5_STORAGE_ERR_TAG_NOT_FOUND ,
    CAN5_STORAGE_ERR_TAG_TOO_LONG  ,
    CAN5_STORAGE_ERR_FILE_NOT_FOUND,
    CAN5_STORAGE_ERR_FILESYSTEM    ,
    CAN5_STORAGE_ERR_JSON          ,
    CAN5_STORAGE_ERR_NO_SPACE      ,
    CAN5_STORAGE_ERR_SEARCH_FAIL   ,
    CAN5_STORAGE_ERR_DIR_NOT_EMPTY ,

    /** Time Errors */
    CAN5_TIME_ERR_MKTIME           ,
    CAN5_TIME_ERR_SET_SYS_TIME     ,
    CAN5_TIME_ERR_GET_SYS_TIME     ,
    CAN5_TIME_ERR_SET_RTC_TIME     ,

    /* UART Errors */
    CAN5_SENSOR_ERR_PARSE_INCOMPLETE,
    CAN5_SENSOR_ERR_PARSE_ERROR    ,
    CAN5_SENSOR_ERR_INVALID_PORT   ,
    CAN5_SENSOR_ERR_BUSY,

    CAN5_NMEA_PARSE_ERROR          ,
    
    /** Sensor Errors */
    CAN5_SENSOR_ERR_BME280_DETECT  ,

    CAN5_SENSOR_ERR_TIMEOUT        ,

    CAN5_CODEC_ERR_PARTITION_NOT_FOUND,
    CAN5_CODEC_ERR_NO_DATAPOINT    ,
    CAN5_CODEC_ERR_INVALID_TOKEN   ,
    CAN5_CODEC_ERR_INVALID_DATATYPE,
    CAN5_CODEC_ERR_VAL_PARSE       ,
    CAN5_CODEC_ERR_NO_TIMESTAMP    ,
    CAN5_CODEC_ERR_INVALID_CODEC   ,
    CAN5_CODEC_ERR_RX_PARSE        ,
    CAN5_CODEC_ERR_RX_INVALID_CRC  ,

    CAN5_HTTP_ERR_AUTH             ,

    CAN5_MISC_ERR_OTA_FAILED       ,

    CAN5_ERR_LAST,
};


/**
  * @brief Returns string for can5_err_t error codes
  *
  *
  * @param code can5_err_t error code
  * @return string error message
  */
const char *can5_err_to_str(can5_err_t code);


/**
 * @brief print the error if is a CAN5 error
 * 
 * @param code  error code to lookup
 * @return true error tag found
 * @return false not a can5 error -> esp error
 */
bool _can5_error_check(can5_err_t code, const char* function, const char* file,  int line, bool abort);


/**
 * @brief Check if is CAN5 error before calling the ESP ERROR CHECK macro
 * 
 */
#define CAN5_ERR_CHECK(x) do {                             \
    int32_t r = x;                                         \
    _can5_error_check(r, __func__, __FILE__, __LINE__, true);\
    /*ESP_ERROR_CHECK(r);*/                                \
} while (0)

/**
 * @brief Print error tag but continue execution
 * 
 */
#define CAN5_ERR_CHECK_NO_ABORT(x) do {                    \
    int32_t r = x;                                         \
    _can5_error_check(r, __func__, __FILE__, __LINE__, false);\
} while (0)


#define VERIFY_pdPASS(x) do {                            \
    int32_t r = x;                                       \
    if (r != pdPASS) return CAN5_ERR_FREERTOS_pdFAIL;    \
} while (0)

#define VERIFY_SUCCESS(x) do {          \
    int32_t r = x;                      \
    if (r != CAN5_SUCCESS) return r;    \
} while (0)

#define VERIFY_SUCCESS_SAFERETURN(x, fail_procedure) do {     \
    int32_t r = x;                                            \
    if (r != CAN5_SUCCESS) {                                  \
        fail_procedure;                                       \
        return r;                                             \
    }\
} while (0)

#define VERIFY_SUCCESS_VOID(x) do {          \
    int32_t r = x;                           \
    if (r != CAN5_SUCCESS) return;           \
} while (0)


#define VERIFY_NOT_NULL(x) do {             \
    if (x == NULL) return CAN5_ERR_NULL;    \
} while (0)

#define VERIFY_NOT_NULL_VOID(x) do {       \
    if (x == NULL) return;                 \
} while (0)


#define VERIFY_ALLOC(ptr, size) {                           \
    if (!(ptr = calloc(size, 1))) {                         \
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;                 \
    }                                                       \
}

#define VERIFY_ALLOC_SAFERETURN(ptr, size, fail_procedure)  {       \
    if (!(ptr = calloc(size, 1))) {                                 \
        fail_procedure;                                             \
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;                         \
    }                                                               \
}

#define VERIFY_ALLOC_SAFENORETURN(ptr, size, fail_procedure)  {     \
    if (!(ptr = calloc(size, 1))) {                                 \
        fail_procedure;                                             \
    }                                                               \
}
#endif // CAN5_ERR_H
