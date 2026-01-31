/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   03/02/22
*
* File:  can5_modules_init.c
* Descr:
*******************************************************************************/

#include <esp_ota_ops.h>
#include <can5_error.h>
#include <esp_log.h>
#include <esp_event.h>
#include <can5_module.h>
#include <can5_storagemng.h>
#include <can5_hal.h>
#include <driver/i2c.h>
#include <can5_rtc.h>
#include <can5_sensormng.h>
#include <can5_netmng.h>
#include <can5_config.h>
#include <can5.h>
#include <string.h>
#include "can5_httpserver.h"
#include "can5_mqtt_client.h"
#include "can5_logger.h"
#include "can5_cmdr.h"
#include "can5_utils.h"

static const char *TAG =        "CAN5_INIT";

#define TRACE_FUNC_START        ESP_LOGI(TAG, "IN -> %s ...", __FUNCTION__)
#define TRACE_FUNC_END          ESP_LOGI(TAG, "OUT <- %s ...", __FUNCTION__);

#define RESET_CONFIG            true
/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static void __can5_shutdown(void);

static can5_err_t __can5_init_logger();

static can5_err_t __can5_init_storage_manager();

static can5_err_t __can5_init_can5_config();

static bool __can5_check_ota_upgrade();

static can5_err_t __can5_init_hal();

static can5_err_t __can5_init_rtc();

static can5_err_t __can5_mqttclient();

static can5_err_t __can5_httpserver();

static can5_err_t __can5_init_sensormng();

static can5_err_t __can5_init_net();

static can5_err_t __can5_init_commander();

static can5_err_t __populate_device_id();

static can5_err_t __can5_init_hal_wifi(bool is_ota);

static can5_err_t __can5_init_hal_cell(bool enable_sntp);

static can5_err_t __can5_update_time();

/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

static portTASK_FUNCTION(__can5_start_cell_task, pv)
{
    CAN5_ERR_CHECK(__can5_init_hal_cell(false));
    vTaskDelete(NULL);
}

/**
 * @brief Initialize Essential Modules in boot-up
 * @return
 */

can5_err_t can5_modules_init(bool *ota_upgrade)
{
    TRACE_FUNC_START;


    ESP_LOGE(TAG, "Byte Order: %d", __BYTE_ORDER__);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    CAN5_ERR_CHECK(can5_register_shutdown_handler(__can5_shutdown));


    VERIFY_SUCCESS(__can5_init_hal());

    VERIFY_SUCCESS(__can5_init_logger());

    VERIFY_SUCCESS(__can5_init_storage_manager());

    VERIFY_SUCCESS(__can5_init_can5_config());

    // let the device continue if RTC encounters failure.
    CAN5_ERR_CHECK_NO_ABORT(__can5_init_rtc());

    *ota_upgrade = __can5_check_ota_upgrade();

#ifdef CONFIG_IDF_TARGET_ESP32S3
    VERIFY_SUCCESS(hal.enable_sens_switch(true));
    hal.pm_enable(true);
#endif

    if (!*ota_upgrade) {
        VERIFY_SUCCESS(can5_init());

        VERIFY_SUCCESS(__can5_init_commander());

        VERIFY_SUCCESS(__populate_device_id());

        VERIFY_SUCCESS(__can5_mqttclient());

        VERIFY_SUCCESS(__can5_httpserver());

        VERIFY_SUCCESS(__can5_init_net());

        VERIFY_SUCCESS(__can5_init_sensormng());
    }

    if (*ota_upgrade) {
        VERIFY_SUCCESS(can5_ota_init());

        VERIFY_SUCCESS(__can5_init_commander());
    }

    VERIFY_SUCCESS(rtc.register_hook(CAN5_RTC_HOOKS_TIME_UPDATE, __can5_update_time));

    VERIFY_SUCCESS(__can5_init_hal_wifi(*ota_upgrade));

    VERIFY_SUCCESS(__can5_init_hal_cell(!*ota_upgrade));

    TRACE_FUNC_END;

    return CAN5_SUCCESS;
}

/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/

static void __can5_shutdown(void)
{
    if (can5_storage_status_get() != 0) {
        // A bit of hack here, 0 means the module is not initialized.
        can5_storage_commit_dictionary_fs();
    }
    hal.shutdown_cb();
    CAN5_MODULE_UINIT(can5_logger)();
}

static can5_err_t __can5_init_storage_manager()
{
    TRACE_FUNC_START;
    bool log_to_sd;

    log_to_sd = false;

    CAN5_ERR_CHECK(can5_storage_init());

#if CONFIG_LOG_DEFAULT_LEVEL_DEBUG != 1

    config_manager.read_bool(CFG_LOG_TO_SD, &log_to_sd);

    if (log_to_sd) {
        ESP_LOGI(TAG, "Logging to SD card...");
        CAN5_ERR_CHECK(can5_logger.activate_stream(CAN5_LOGGER_STREAM_SD, NULL));
    }
#endif


    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}


static can5_err_t __can5_init_logger()
{
    TRACE_FUNC_START;
    CAN5_MODULE_INIT(can5_logger)();

    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_init_can5_config()
{
    TRACE_FUNC_START;

    //config_manager.factory_default();

    CAN5_ERR_CHECK(CAN5_MODULE_INIT(config_manager)());

    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_init_hal_wifi(bool is_ota)
{
    TRACE_FUNC_START;
    bool wifi_sta_enable, wifi_ap_enable;
    char *sta_ssid, *sta_password;
    char *ap_ssid, *ap_password;
    bool enable_sntp = !is_ota;

    char *sntp_server;

    VERIFY_SUCCESS(config_manager.read_bool(CFG_WIFI_STA_ENABLE, &wifi_sta_enable));
    VERIFY_SUCCESS(config_manager.read_bool(CFG_WIFI_AP_ENABLE, &wifi_ap_enable));
    VERIFY_SUCCESS(config_manager.read(CFG_WIFI_STA_SSID, (uint8_t **)&sta_ssid, NULL));
    VERIFY_SUCCESS(config_manager.read(CFG_WIFI_STA_PASS, (uint8_t **)&sta_password, NULL));
    VERIFY_SUCCESS(config_manager.read(CFG_DEVICE_NAME, (uint8_t **)&ap_ssid, NULL));
    VERIFY_SUCCESS(config_manager.read(CFG_WIFI_AP_PASS, (uint8_t **)&ap_password, NULL));

    sntp_server = NULL;
    if (config_manager.read(CFG_WIFI_SNTP_SERVER, (uint8_t **)&sntp_server, NULL) != CAN5_SUCCESS) {
        // default address, for the worst case
        sntp_server = strdup("pool.ntp.org");
    }

    if (wifi_sta_enable || wifi_ap_enable ) {
        VERIFY_SUCCESS(hal.wifi_start(is_ota? false: wifi_ap_enable, // do not start ap in ota mode
                                      wifi_sta_enable,
                                      sta_ssid, sta_password,
                                      ap_ssid, ap_password,
                                      enable_sntp, sntp_server));
    }


    free(sta_ssid);
    free(sta_password);
    free(ap_ssid);
    free(ap_password);
    free(sntp_server);

    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_init_hal_cell(bool enable_sntp)
{
    TRACE_FUNC_START;
    can5_err_t ret;
    bool cell_enabled;
    char *sntp_server;
    char *apn;
    bool enable_gps;

    ret = CAN5_SUCCESS;
    VERIFY_SUCCESS(config_manager.read_bool(CFG_CELL_ENABLE, &cell_enabled));
    VERIFY_SUCCESS(config_manager.read_bool(CFG_CELL_ENABLE_GPS, &enable_gps));

    if (!cell_enabled & !enable_gps) {
        goto done;
    }

    sntp_server = NULL;
    if (config_manager.read(CFG_WIFI_SNTP_SERVER, (uint8_t **)&sntp_server, NULL) != CAN5_SUCCESS) {
        // default address, for the worst case
        sntp_server = strdup("pool.ntp.org");
    }

    apn = NULL;
    if (config_manager.read(CFG_CELL_APN, (uint8_t **)&apn, NULL) != CAN5_SUCCESS) {
        // default address, for the worst case
        apn = strdup("internet");
    }


#if CONFIG_IDF_TARGET_ESP32S3
    hal.enable_net_switch(true);
#endif

    VERIFY_SUCCESS(hal.cell_start(cell_enabled, enable_gps, apn, enable_sntp, sntp_server));

    free(apn);
    free(sntp_server);

done:
    TRACE_FUNC_END;
    return ret;
}

static can5_err_t __can5_init_hal()
{
    TRACE_FUNC_START;
    CAN5_ERR_CHECK(CAN5_MODULE_INIT(hal)());

    for (uint8_t i = 1; i < 127; i++) {
        int ret;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 100 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at: 0x%2x\n", i);
        }
    }
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}


static bool __can5_check_ota_upgrade()
{
    int64_t ota_upgrade;
    CAN5_ERR_CHECK(config_manager.read_int(CFG_OTA_MODE, &ota_upgrade));

    return ota_upgrade != 0;
}

static can5_err_t __can5_init_rtc()
{
    TRACE_FUNC_START;
    //time_t now;
    CAN5_ERR_CHECK_NO_ABORT(CAN5_MODULE_INIT(rtc)());
    // VERIFY_SUCCESS(rtc.register_hook(CAN5_RTC_HOOKS_PRE_SLEEP, __can5_pre_sleep));
    //VERIFY_SUCCESS(rtc.register_hook(CAN5_RTC_HOOKS_POST_SLEEP, __can5_post_sleep));
    VERIFY_SUCCESS(rtc.print_rtc_time());
    //__get_now(&now);

    /*if (now < 0) {
        // if we cross the yk38 problem mark, as esp handles time in 32 bit
        ESP_LOGE(TAG, "RTC time is invalid, set to default!");
        rtc.reset_to_default_time();
        rtc.rtc_to_sys();
        VERIFY_SUCCESS(rtc.print_rtc_time());
    }
     */

    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_mqttclient()
{
    TRACE_FUNC_START;
    CAN5_ERR_CHECK(can5_mqtt_client_init());
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_httpserver()
{
    TRACE_FUNC_START;
    CAN5_ERR_CHECK(CAN5_MODULE_INIT(httpserver)());
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_init_sensormng()
{
    TRACE_FUNC_START;
    CAN5_ERR_CHECK(CAN5_MODULE_INIT(sensor_manager)());
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}


static can5_err_t __can5_init_net()
{
    TRACE_FUNC_START;
    CAN5_ERR_CHECK(CAN5_MODULE_INIT(net)());
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_init_commander()
{
    TRACE_FUNC_START;
    CAN5_ERR_CHECK(CAN5_MODULE_INIT(can5_commander)());
    TRACE_FUNC_END;
    return CAN5_SUCCESS;

}

static can5_err_t __populate_device_id()
{
    TRACE_FUNC_START;
    uint64_t device_id;
    uint8_t mac[6];

    /* Get mac_id */
    VERIFY_SUCCESS(esp_efuse_mac_get_default((uint8_t *)&mac));

    device_id = 0;
    for (int i = 5; i >= 0; i--) {
        device_id |= ((uint64_t )mac[i]) << ((5 - i) * 8);
    }

    VERIFY_SUCCESS(config_manager.write_int(CFG_DEVICE_ID, device_id));
    ESP_LOGI(TAG, "Device ID: %llu", device_id);

    const esp_app_desc_t *desc = esp_ota_get_app_description();
    ESP_LOGI(TAG, "Canarin 5 Firmware Version: %s", desc->version);
    VERIFY_SUCCESS(config_manager.write(CFG_APP_VERSION, (uint8_t *)desc->version, strlen(desc->version)));

    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}

static can5_err_t __can5_update_time()
{
    TRACE_FUNC_START;
    can5_err_t ret;
    ret = can5_commander.add_cmd(CAN5_CMD_RECALC_JOBS, NULL, NULL, NULL);
    TRACE_FUNC_END;
    return ret;
}
