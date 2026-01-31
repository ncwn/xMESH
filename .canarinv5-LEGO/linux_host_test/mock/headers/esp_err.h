/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   30/01/22
*
* File:  esp_err.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_ESP_ERR_H
#define TEST_APP_ESP_ERR_H

enum can5_err_list_e;

typedef enum can5_err_list_e esp_err_t;

#define ESP_OK      0
#define  ESP_FAIL   -1

static inline const char *esp_err_to_name(int code)
{
    return "mock esp_err_to_name()";
}

#endif //TEST_APP_ESP_ERR_H
