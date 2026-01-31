
/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   24/01/22
*
* File:  can5.c
* Descr:
*******************************************************************************/
#include <sys/cdefs.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/i2c.h"
#include "can5_config_provider.h"
#include "can5_utils.h"
#include "can5_module.h"
#include "can5_events.h"
#include "can5_storagemng.h"
#include "can5_hal.h"
#include "can5_rtc.h"
#include "can5_netmng.h"
#include "can5_config.h"
#include "can5_sensormng.h"
#include "can5.h"
#include "can5_logger.h"
#include "can5_cmdr.h"

static const char *TAG =        "CAN5";

#define TRACE_FUNC_START   ESP_LOGI_V(TAG, "IN -> %s ...", __FUNCTION__)
#define TRACE_FUNC_END    ESP_LOGI_V(TAG, "OUT <- %s ...", __FUNCTION__);

//#define RUN_PRE_SLEEP_HOOK(mod)  { if (mod.is_sleepable() && !mod.is_sleeping()) CAN5_ERR_CHECK_NO_ABORT(mod.pre_sleep()); }
//#define RUN_POST_SLEEP_HOOK(mod)  { if (mod.is_sleepable() && mod.is_sleeping()) CAN5_ERR_CHECK_NO_ABORT(mod.post_sleep()); }
#define RUN_PRE_SLEEP_HOOK(mod)  CAN5_ERR_CHECK_NO_ABORT(mod.pre_sleep())
#define RUN_POST_SLEEP_HOOK(mod)  CAN5_ERR_CHECK_NO_ABORT(mod.post_sleep())

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
    StackType_t stack[CONFIG_CAN5_MONITOR_TASK_STACK_SIZE];
} task_t;

static struct {
    struct {
        esp_event_handler_instance_t sensormng_evt;
        esp_event_handler_instance_t net_evt;
        esp_event_handler_instance_t hal_evt;
    } evt;

    struct {
        esp_timer_handle_t commit_fs_timer;
#if CONFIG_CAN5_SHOW_FREE_HEAP_MEMORY
        esp_timer_handle_t heap_mem_check_timer;
#endif
    } timers;
} can5;



/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

#if CONFIG_CAN5_SHOW_FREE_HEAP_MEMORY
static void __timer_cb_heap_mem(void* arg);
static can5_err_t __print_heap_usage(void *param);
#endif


__attribute__((unused)) static can5_err_t __can5_pre_sleep();

__attribute__((unused)) static can5_err_t __can5_post_sleep();


static void __sensormng_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data);
static void __net_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data);

static void __hal_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data);

/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

__attribute__((unused)) static bool __check_sleepable()
{
    return CAN5_CHECK_SLEEPABLE(sensor_manager) && CAN5_CHECK_SLEEPABLE(net);
}


void RTC_IRAM_ATTR esp_wake_deep_sleep(void) {
    esp_default_wake_deep_sleep();
    // Add additional functionality here
}


can5_err_t can5_init()
{
    esp_event_handler_instance_register(CAN5_EVT_SENSORMNG,
                                        ESP_EVENT_ANY_ID,
                                        __sensormng_evt_handler,
                                        (void *) &can5,
                                        &can5.evt.sensormng_evt);

    esp_event_handler_instance_register(CAN5_EVT_NET,
                                        ESP_EVENT_ANY_ID,
                                        __net_evt_handler,
                                        (void *) &can5,
                                        &can5.evt.net_evt);

    esp_event_handler_instance_register(CAN5_EVT_HAL,
                                        ESP_EVENT_ANY_ID,
                                        __hal_evt_handler,
                                        (void *) &can5,
                                        &can5.evt.hal_evt);

    return CAN5_SUCCESS;
}

static void __commit_to_disk_cb(can5_err_t ret, can5_cmd_params_t *result, void *user_data)
{
    CAN5_ERR_CHECK(ret);
}

static void __timer_commit_config_to_disk(void* arg)
{
    can5_commander.add_cmd(CAN5_CMD_COMMIT_FS_DICTIONARY, NULL, __commit_to_disk_cb, NULL);
}

static portTASK_FUNCTION(__can5_ota_upgrade, pv)
{
    CAN5_ERR_CHECK(can5_ota_upgrade());
    can5_restart();
    vTaskDelete(NULL);
}

void can5_run(bool ota_upgrade)
{
    if (ota_upgrade) {
        TaskHandle_t upgrade_task;

        xTaskCreate(__can5_ota_upgrade,
                    "ota",
                    CONFIG_ESP_MAIN_TASK_STACK_SIZE,
                    NULL,
                    12,
                    &upgrade_task);

    }
    else {
        const esp_timer_create_args_t commit_fs_timer_args = {
            .callback = &__timer_commit_config_to_disk,
            .name = "commit to disk",
            .skip_unhandled_events = true,
        };

        CAN5_ERR_CHECK(esp_timer_create(&commit_fs_timer_args, &can5.timers.commit_fs_timer));

        esp_timer_start_periodic(can5.timers.commit_fs_timer, CONFIG_CAN5_COMMIT_FS_INTERVAL_MS * 1000);

#if CONFIG_CAN5_SHOW_FREE_HEAP_MEMORY
        const esp_timer_create_args_t heap_mem_check_timer_args = {
            .callback = &__timer_cb_heap_mem,
            .name = "heap mem check",
            .skip_unhandled_events = true,
        };

        CAN5_ERR_CHECK(esp_timer_create(&heap_mem_check_timer_args, &can5.timers.heap_mem_check_timer));
        esp_timer_start_periodic(can5.timers.heap_mem_check_timer, 1000 * 1000);
#endif
    }

    can5_commander_loop();
}



/* ---------------------------------------------------------------------
 * Event Handlers
 -----------------------------------------------------------------------*/
static void __sensormng_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data)
{
    if (event_base != CAN5_EVT_SENSORMNG) return;
    switch (event_id) {
        case CAN5_SENSORMNG_EVT_COMPLETE:
            break;

        case CAN5_SENSORMNG_EVT_BUSY:
            break;
    }

}

static void __net_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data)
{
    if (event_base != CAN5_EVT_NET) return;
    switch (event_id) {
        case CAN5_NET_EVT_RUNNING:
            break;

        case CAN5_NET_EVT_BUSY:
            break;

        case CAN5_NET_EVT_COMPLETED:
            break;

    }

}


static can5_cmd_params_t cmd_params;
static void __hal_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                void *event_data)
{
    if (event_base != CAN5_EVT_HAL) return;

    can5_hal_evt_usrbtn_data_t *btn_evt_data;
    char *operator;

    switch (event_id) {
        case CAN5_HAL_EVT_WIFI_STA_CONNECTED:
        case CAN5_HAL_EVT_CELL_CONNECTED:
            ESP_LOGI(TAG, "Wifi is connected.");
            can5_commander.add_cmd(CAN5_CMD_ACTIVATE_NETWORK_LOGGERS, NULL, NULL, NULL);
            break;
        case CAN5_HAL_EVT_CELL_OPERATOR:
            operator = (char *)event_data;
            config_manager.write(CFG_CELL_OPERATOR, (uint8_t *)operator, strlen(operator));
            break;
        case CAN5_HAL_EVT_USRBTN_LONG_PRESS:
            ESP_LOGI(TAG, "Long press reboot in AP mode.");
            can5_commander.add_cmd(CAN5_CMD_ENABLE_WIFI_AP, NULL, NULL, NULL);

            cmd_params.restart_after = 1;
            can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &cmd_params, NULL, NULL);
            break;
        case CAN5_HAL_EVT_USRBTN_PRESS:
            btn_evt_data = event_data;
            if (btn_evt_data->count == 9) {
                can5_commander.add_cmd(CAN5_CMD_FACTORY_RESET, NULL, NULL, NULL);

                cmd_params.restart_after = 1;
                can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &cmd_params, NULL, NULL);
            }
    }
}

/* ---------------------------------------------------------------------
 * Private Functions
 -----------------------------------------------------------------------*/
#if CONFIG_CAN5_SHOW_FREE_HEAP_MEMORY
static void __timer_cb_heap_mem(void* arg)
{
    can5_cmd_params_t params = {
        .run_cb = {
            .run_cb_param = NULL,
            .run_cb = __print_heap_usage,
        }
    };

    can5_commander.add_cmd(CAN5_CMD_RUN_CB, &params, NULL, NULL);

}
#endif

__attribute__((unused)) static can5_err_t __can5_pre_sleep()
{
    TRACE_FUNC_START;
    RUN_PRE_SLEEP_HOOK(hal.module);
    RUN_PRE_SLEEP_HOOK(rtc.module);
    //RUN_PRE_SLEEP_HOOK(net.module);
    RUN_PRE_SLEEP_HOOK(config_manager.module);
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}


__attribute__((unused)) static can5_err_t __can5_post_sleep()
{
    TRACE_FUNC_START;
    RUN_POST_SLEEP_HOOK(hal.module);
    RUN_POST_SLEEP_HOOK(rtc.module);
    //RUN_POST_SLEEP_HOOK(net.module);
    RUN_POST_SLEEP_HOOK(config_manager.module);
    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}


/* ---------------------------------------------------------------------
 * Debug Support
 -----------------------------------------------------------------------*/
#define LEAK_NOTIFY_THRESHOLD       256
static can5_err_t __print_heap_usage(void *param)
{
    TRACE_FUNC_START;

    static size_t last_size = 0;
    size_t new_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

    if (abs((int)last_size - (int)new_size) >= LEAK_NOTIFY_THRESHOLD) {
        ESP_LOGI(TAG, "Free Mem (Check Possible Mem Leak): %d" , new_size);
        last_size = new_size;
    }
    rtc.print_rtc_time();

    hal.print_cell_status();

    TRACE_FUNC_END;
    return CAN5_SUCCESS;
}
