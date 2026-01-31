/**************************************************
 * Author: rmukhia
 * Creation Date: 6/9/22
 * Description: 
 **************************************************/
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_event.h>
#include "can5_error.h"
#include "can5_config.h"
#include "can5_utils.h"
#include "can5_events.h"

static const char *TAG = "OTA";
#define OTA_WIFI_CONNECT_TIMEOUT    (1000 * 60) //1 minute


#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif


static struct {
    esp_event_handler_instance_t hal_evt;
    EventGroupHandle_t evt_group;
} can5_ota;

typedef enum ota_upgrade_evt_e {
    NET_CONNECTED = 0,
} ota_upgrade_evt_e;

static void __hal_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data);
can5_err_t can5_ota_init()
{
    ESP_LOGI(TAG, "OTA Upgrade mode");

    esp_event_handler_instance_register(CAN5_EVT_HAL,
                                        ESP_EVENT_ANY_ID,
                                        __hal_evt_handler,
                                        NULL,
                                        &can5_ota.hal_evt);
    can5_ota.evt_group = xEventGroupCreate();

    VERIFY_NOT_NULL(can5_ota.evt_group);

    return CAN5_SUCCESS;
}


static void __hal_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data)
{

    if (event_base == CAN5_EVT_HAL) {
        switch(event_id) {
            case CAN5_HAL_EVT_WIFI_STA_CONNECTED:
            case CAN5_HAL_EVT_CELL_CONNECTED:
                ESP_LOGI(TAG, "Wifi is connected.");
                xEventGroupSetBits(can5_ota.evt_group, BIT(NET_CONNECTED));
                break;
            default:
                break;
        }
    }
}
/**********************
 * OTA related
 **********************/
static const char *binary_name = "can5-app.bin";

can5_err_t can5_ota_upgrade()
{
    TRACE_FUNC;

    char *url;
    esp_http_client_config_t http_config;
    esp_https_ota_config_t ota_config;
    esp_err_t ret;

    EventBits_t bits = xEventGroupWaitBits(can5_ota.evt_group,
                        BIT(NET_CONNECTED) ,
                        pdTRUE,
                        pdFALSE,
                        pdMS_TO_TICKS(OTA_WIFI_CONNECT_TIMEOUT));

    VERIFY_SUCCESS(config_manager.write_int(CFG_OTA_MODE, 0));

    // if we do not get net connection
    if ((bits & BIT(NET_CONNECTED)) == 0) {
        ESP_LOGE(TAG, "Wifi connection could not be establised for OTA upgrade.");
        return CAN5_ERROR;
    }

    url = NULL;
    VERIFY_SUCCESS(config_manager.read(CFG_OTA_UPDATE_URL, (uint8_t **) &url, NULL));

    strcat(url, binary_name);

    ESP_LOGI(TAG, "Performing OTA update from %s", url);

    size_t new_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Free Mem (Check Possible Mem Leak): %d" , new_size);

    CLEAR_STRUCT(http_config);
    http_config.url = url;
    http_config.crt_bundle_attach = esp_crt_bundle_attach;

    CLEAR_STRUCT(ota_config);
    ota_config.http_config = &http_config;
    //ota_config.partial_http_download = true;
    //ota_config.max_http_request_size = CONFIG_CAN5_OTA_MAX_HTTPS_SIZE;

    esp_https_ota_handle_t https_ota_handle = NULL;
    ret = esp_https_ota_begin(&ota_config, &https_ota_handle);

    if (https_ota_handle == NULL) {
        return ESP_FAIL;
    }

    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }

    if (ret != ESP_OK) {
        esp_https_ota_abort(https_ota_handle);
        return ret;
    }

    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if (ota_finish_err != ESP_OK) {
        return ota_finish_err;
    }

    free(url);

    ESP_LOGI(TAG, "****************************** OTA Update complete ****************************");

    return CAN5_SUCCESS;
}
