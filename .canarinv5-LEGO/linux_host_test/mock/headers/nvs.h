/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   10/02/22
*
* File:  nvs.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_NVS_H
#define TEST_APP_NVS_H

typedef enum {
    NVS_READONLY,
    NVS_READWRITE
} ESP_NVS_MODE;

typedef int nvs_handle_t;
esp_err_t nvs_open(const char *tag, int mode, nvs_handle_t *hdl);
esp_err_t nvs_set_blob(nvs_handle_t hdl, const char *key, const void *data, const size_t len);
esp_err_t nvs_get_blob(nvs_handle_t hdl, const char *key, void *data, size_t *len);
esp_err_t nvs_close(nvs_handle_t hdl);
esp_err_t nvs_commit(nvs_handle_t hdl);
esp_err_t nvs_erase_key(nvs_handle_t hdl, const char *data);
esp_err_t nvs_erase_all(nvs_handle_t hdl);
esp_err_t nvs_set_i32(nvs_handle_t hdl, const char *key, int32_t data);
esp_err_t nvs_get_i32(nvs_handle_t hdl, const char *key, int32_t *data);

#endif //TEST_APP_NVS_H
