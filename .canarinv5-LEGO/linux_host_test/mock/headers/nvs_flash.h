/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   10/02/22
*
* File:  nvs_flash.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_NVS_FLASH_H
#define TEST_APP_NVS_FLASH_H
#include "esp_err.h"

#define ESP_ERR_NVS_NO_FREE_PAGES ESP_FAIL
#define ESP_ERR_NVS_NEW_VERSION_FOUND ESP_FAIL

esp_err_t  nvs_flash_init();
esp_err_t  nvs_flash_erase();


#endif //TEST_APP_NVS_FLASH_H
