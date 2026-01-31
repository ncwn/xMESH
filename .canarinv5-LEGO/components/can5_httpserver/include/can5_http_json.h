/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   28/02/22
*
* File:  can5_http_json.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_CAN5_HTTP_JSON_H
#define TEST_APP_CAN5_HTTP_JSON_H

#include "can5_error.h"
#include "can5_config.h"
#include "cJSON.h"


typedef enum can5_http_json_type_e {
    CAN5_HTTP_JSON_GENERAL,
    CAN5_HTTP_JSON_COMMUNICATION,
    CAN5_HTTP_JSON_WIFI_AP,
    CAN5_HTTP_JSON_WIFI_STA,
    CAN5_HTTP_JSON_CELLULAR,
    CAN5_HTTP_JSON_LORAWAN,
    CAN5_HTTP_JSON_IO,
    CAN5_HTTP_JSON_CRONTAB,
    CAN5_HTTP_JSON_LOGGING,

} can5_http_json_type_t;

char *can5_http_json_str_get(can5_http_json_type_t type);

char *can5_http_json_set(can5_http_json_type_t type, const char *body);

bool can5_validate_param(can5_cfg_type_t type, const char *param, bool *skip);

char *__get_formatted_mac_address(const uint8_t * mac);

#endif //TEST_APP_CAN5_HTTP_JSON_H
