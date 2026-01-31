/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   30/01/22
*
* File:  mock.c
* Descr:
*******************************************************************************/

#include <can5_config_provider.h>
#include <stdio.h>
#include <malloc.h>
#include <esp_event.h>
#include <can5_netif.h>
#include <can5_cmdr.h>
#include "can5_netproto.h"
#include "can5_rtc.h"
#include "nvs.h"

#define MOCK_RET_OK(dec)        dec { return ESP_OK; }

can5_rtc_t rtc = {

};

can5_netif_t  netif_wifi;
can5_netif_t  netif_sim7600;
can5_netif_t  netif_lwan;

MOCK_RET_OK(esp_err_t cmdr_add_cmd(can5_cmd_type_t a, can5_cmd_params_t *b, can5_cmd_cb cb, void *d));

can5_cmdr_t can5_commander = {
    .add_cmd = cmdr_add_cmd
};

can5_netproto_t netstrat_hazemon;
can5_netproto_t netstrat_lwan;

ESP_EVENT_DEFINE_BASE(CAN5_EVT_SCHEDULER);
/*
can5_cfg_res_t get_config(const can5_cfg_cmd_type_t type, size_t *len)
{
    can5_cfg_res_t config;
    switch (type) {

        case CAN5_CFG_REQ_DEVICE_ID:
            config.device_id = calloc(sizeof(can5_cfg_res_sensor_id_t), 1);
            config.device_id->project_id = 1;
            config.device_id->id = 39628670564992;
            break;
        default:
            printf("INVALID config, add config here %s %s %s", __FILE__, __FUNCTION__ , __LINE__);
    }

    return config;
}
*/

MOCK_RET_OK(esp_err_t  nvs_flash_init());
MOCK_RET_OK(esp_err_t  nvs_flash_erase());
MOCK_RET_OK(esp_err_t nvs_open(const char *tag, int mode, nvs_handle_t *hdl));
MOCK_RET_OK(esp_err_t nvs_set_blob(nvs_handle_t hdl, const char *key, const void *data, const size_t len));
MOCK_RET_OK(esp_err_t nvs_get_blob(nvs_handle_t hdl, const char *key, void *data, size_t *len));
MOCK_RET_OK(esp_err_t nvs_close(nvs_handle_t hdl));
MOCK_RET_OK(esp_err_t nvs_commit(nvs_handle_t hdl));
MOCK_RET_OK(esp_err_t nvs_set_i32(nvs_handle_t hdl, const char *key, int32_t data));
MOCK_RET_OK(esp_err_t nvs_get_i32(nvs_handle_t hdl, const char *key, int32_t *data));
MOCK_RET_OK(esp_err_t nvs_erase_key(nvs_handle_t hdl, const char *data));
MOCK_RET_OK(esp_err_t nvs_erase_all(nvs_handle_t hdl));



MOCK_RET_OK(esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int, esp_event_handler_t handler,
                                                          void * args, esp_event_handler_instance_t instance));


MOCK_RET_OK(esp_err_t esp_event_post(esp_event_base_t event_base,
                         int32_t event_id,
                         void *event_data,
                         size_t event_data_size,
                         TickType_t ticks_to_wait));

