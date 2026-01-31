/**************************************************
 * Author: rmukhia
 * Creation Date: 15/7/22
 * Description:  Miscellaneous features.
 **************************************************/
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_crt_bundle.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <lwip/netdb.h>
#include <ping/ping_sock.h>
#include "can5_cmdr.h"
#include "can5_utils.h"
#include "can5_config.h"
#include "can5_logger.h"
#include "can5_storagemng.h"
#include "can5_sensor_data.h"
#include "can5_cron.h"
#include "can5_events.h"
#include "can5_hal.h"

static const char *TAG = "COMMANDER";

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define CMD_FUNCTION(name, param)  can5_err_t name(can5_cmd_params_t *param)

static can5_err_t init();

static can5_err_t uninit();

static can5_err_t add_cmd(can5_cmd_type_t type, can5_cmd_params_t *param,
                          can5_cmd_cb cb, void *user_data);

can5_cmdr_t can5_commander = {
    .module = {
        .init = init,
        .uninit = uninit,
    },
    .add_cmd = add_cmd,
};

typedef enum cmdr_status_e {
    CMDR_STATUS_UNINITD,
    CMDR_STATUS_INITD,
} cmdr_status_t;


typedef struct can5_cmd_s {
    can5_cmd_type_t type;
    can5_cmd_fn cmd_fn;
    can5_cmd_params_t params;
    void *user_data;
    can5_cmd_cb cmd_cb;
} can5_cmd_t;

static struct {
    volatile cmdr_status_t status;
    time_t timer_time;              // the time at which timer should have been called.
    QueueHandle_t q_hdl;
    esp_timer_handle_t cron_timer;
} __cmdr = {
    .status = CMDR_STATUS_UNINITD,
    .q_hdl = NULL,
};

ESP_EVENT_DEFINE_BASE(CAN5_EVT_CMDR);

static CMD_FUNCTION(__commit_fs_dictionary, param);
static CMD_FUNCTION(__activate_network_loggers, param);
static CMD_FUNCTION(__wifi_scan, param);
static CMD_FUNCTION(__net_ping, param);
static CMD_FUNCTION(__reset_after, param);
static CMD_FUNCTION(__enable_wifi_ap, param);
static CMD_FUNCTION(__do_factory_reset, param);
static CMD_FUNCTION(__clear_all_sensor_data, param);
static CMD_FUNCTION(__clear_old_sensor_data, param);
static CMD_FUNCTION(__post_event, param);
static CMD_FUNCTION(__recalc_jobs, param);
static CMD_FUNCTION(__run_cb, param);

can5_cmd_t __cmds[] = {
    {
        .type = CAN5_CMD_COMMIT_FS_DICTIONARY,
        .cmd_fn = __commit_fs_dictionary,
    },
    {
        .type = CAN5_CMD_ACTIVATE_NETWORK_LOGGERS,
        .cmd_fn = __activate_network_loggers,
    },
    {
        .type = CAN5_CMD_SCAN_WIFI,
        .cmd_fn = __wifi_scan,
    },
    {
        .type = CAN5_CMD_PING_IP,
        .cmd_fn = __net_ping,
    },
    {
        .type = CAN5_CMD_RESET_AFTER,
        .cmd_fn = __reset_after,
    },
    {
        .type = CAN5_CMD_ENABLE_WIFI_AP,
        .cmd_fn = __enable_wifi_ap,
    },
    {
        .type = CAN5_CMD_FACTORY_RESET,
        .cmd_fn = __do_factory_reset,
    },
    {
        .type = CAN5_CMD_CLEAR_ALL_SENSOR_DATA,
        .cmd_fn = __clear_all_sensor_data,
    },
    {
        .type = CAN5_CMD_CLEAR_OLD_SENSOR_DATA,
        .cmd_fn = __clear_old_sensor_data,
    },
    {
        .type = CAN5_CMD_POST_EVENT,
        .cmd_fn = __post_event,
    },
    {
        .type = CAN5_CMD_RECALC_JOBS,
        .cmd_fn = __recalc_jobs,
    },
    {
        .type = CAN5_CMD_RUN_CB,
        .cmd_fn = __run_cb,
    }
};

static void cron_timer_cb(void *arg);

static can5_err_t init()
{
    TRACE_FUNC;

    __cmdr.q_hdl = xQueueCreate(CONFIG_CAN5_CMD_QUEUE_SIZE, sizeof(can5_cmd_t));
    VERIFY_NOT_NULL(__cmdr.q_hdl);

    esp_timer_create_args_t timer_args = {
        .skip_unhandled_events = true,
        .name = "cron_timer",
        .callback = cron_timer_cb,
        .arg = &__cmdr.timer_time,
    };
    VERIFY_SUCCESS(esp_timer_create(&timer_args, &__cmdr.cron_timer));

    VERIFY_SUCCESS(can5_cron_init());

    __cmdr.status = CMDR_STATUS_INITD;
    return CAN5_SUCCESS;

}

static can5_err_t uninit()
{
    TRACE_FUNC;

    return CAN5_SUCCESS;
}

static can5_err_t add_cmd(can5_cmd_type_t type, can5_cmd_params_t *param,
                          can5_cmd_cb cb, void *user_data)
{
    TRACE_FUNC;

    for (size_t i = 0; i < sizeof(__cmds) / sizeof(__cmds[0]); i++) {
        if (type == __cmds[i].type) {
            can5_cmd_t cmd = __cmds[i];
            if (param) {
                cmd.params = *param;
            }
            else {
                CLEAR_STRUCT(cmd.params);
            }
            cmd.cmd_cb = cb;
            cmd.user_data = user_data;

            if (xQueueSend(__cmdr.q_hdl, &cmd, pdMS_TO_TICKS(500)) == pdFALSE) {
                return CAN5_ERR_FREERTOS_pdFAIL;
            }

            return CAN5_SUCCESS;
        }
    }

    return CAN5_ERR_INVALID_PARAM;
}

static can5_err_t start_cron()
{
    TRACE_FUNC;

    time_t curr_time, next_time;
    curr_time = time(NULL);
    VERIFY_SUCCESS(can5_cron_next_time(curr_time, &next_time));
    __cmdr.timer_time = next_time;

    time_t offset = (next_time - curr_time);
    ESP_LOGI(TAG, "Timer after %ld", offset);
    return esp_timer_start_once(__cmdr.cron_timer, ((uint64_t )offset) * 1000000);
}

static void cron_timer_cb(void *arg)
{
    TRACE_FUNC;

    time_t curr_time = *(time_t *)arg;
    time_t next_time;

    can5_cron_run_jobs(curr_time);

    curr_time = time(NULL);

    if (can5_cron_next_time(curr_time, &next_time) == CAN5_SUCCESS) {
        time_t offset = (next_time - curr_time);
        __cmdr.timer_time = next_time;
        ESP_LOGI(TAG, "Timer after %ld sec", offset);
        if (esp_timer_is_active(__cmdr.cron_timer)) {
            esp_timer_stop(__cmdr.cron_timer);
        }
        esp_timer_start_once(__cmdr.cron_timer, ((uint64_t)offset) * 1000000);
    }
}

static can5_cmd_t cmd;
static can5_err_t ret;

can5_err_t can5_commander_loop(void)
{
    TRACE_FUNC;

    start_cron();

    for (;;) {

        CLEAR_STRUCT(cmd);
        if (xQueueReceive(__cmdr.q_hdl, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ret = cmd.cmd_fn(&cmd.params);

        PRINT_TASK_HIGHWATER_MARK(NULL);
        if (cmd.cmd_cb) {
            cmd.cmd_cb(ret, &cmd.params, cmd.user_data);
        }

        PRINT_TASK_HIGHWATER_MARK(NULL);
    }
}


/***********************************************************************
*  Commands
************************************************************************/

/**********************
 *  Critical Commands
 **********************/

static CMD_FUNCTION(__commit_fs_dictionary, param)
{
    TRACE_FUNC;

    return config_manager.commit_config_to_disk();
}

static CMD_FUNCTION(__post_event, param)
{
    TRACE_FUNC;

    ESP_LOGI(TAG, "Posting event %s", can5_cmdr_evt_getstr(param->cmdr_event.event));

    return esp_event_post(CAN5_EVT_CMDR, param->cmdr_event.event,
                   &param->cmdr_event,
                   sizeof(param->cmdr_event),
                   pdMS_TO_TICKS(param->cmdr_event.timeout_ms));
}

static CMD_FUNCTION(__recalc_jobs, param)
{
    TRACE_FUNC;
    time_t now = time(NULL);
    ESP_LOGI(TAG, "Recalculating jobs timer.");
    cron_timer_cb(&now);
    return CAN5_SUCCESS;
}

/**********************
 * WiFi and net related
 **********************/
static CMD_FUNCTION(__activate_network_loggers, param)
{
    TRACE_FUNC;

    can5_logger_activate_params_t params;
    char *ip;
    int64_t port;
    bool net_sock_active;

    VERIFY_SUCCESS(config_manager.read_bool(CFG_LOG_TO_NETSOCK, &net_sock_active));

    if (net_sock_active) {

        VERIFY_SUCCESS(config_manager.read_int(CFG_LOG_TO_NETSOCK_PORT, &port));

        VERIFY_SUCCESS(config_manager.read(CFG_LOG_TO_NETSOCK_IP, (uint8_t **)&ip, NULL));

        CLEAR_STRUCT(params);

        strcpy(params.net_socket.dest_ip, ip);

        params.net_socket.port = port;

        ESP_LOGI(TAG, "Activating netsock logger to (%s:%lld)",
                 ip,
                 port);

        can5_logger.activate_stream(CAN5_LOGGER_STREAM_NET_SOCKET, &params);

        free(ip);
    }

    return CAN5_SUCCESS;
}

static CMD_FUNCTION(__wifi_scan, param)
{
    TRACE_FUNC;

    can5_cmd_wifi_scan_params_t *scan_params = &param->wifi_scan;
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 0,
                .max = 0,
            },
        },
    };

    esp_wifi_scan_start(&scan_config, true);
    VERIFY_SUCCESS(esp_wifi_scan_get_ap_records(&scan_params->max_scan, scan_params->ap_info));
    VERIFY_SUCCESS(esp_wifi_scan_get_ap_num(&scan_params->ap_count));

    return CAN5_SUCCESS;
}

static void __test_on_ping_success(esp_ping_handle_t hdl, void *args);
static void __test_on_ping_timeout(esp_ping_handle_t hdl, void *args);
static void __test_on_ping_end(esp_ping_handle_t hdl, void *args);

typedef struct net_ping_args_s {
    char *result;
    SemaphoreHandle_t sem;
} net_ping_args_t;

static CMD_FUNCTION(__net_ping, param)
{
    TRACE_FUNC;

    can5_cmd_ping_ip_params_t *ping_params = &param->ping_ip;
    net_ping_args_t args;
    esp_ping_handle_t ping_handle;
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo *res = NULL;

    CLEAR_STRUCT(target_addr);
    CLEAR_STRUCT(hint);

    if (getaddrinfo(ping_params->address, NULL, &hint, &res) != 0) {
        printf("ping: unknown host %s\n", ping_params->address);
        return CAN5_ERR_INVALID_PARAM;
    }
    struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    freeaddrinfo(res);

    ping_handle = NULL;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 8;
    ping_config.timeout_ms = 10000;

    args.result = ping_params->result;


    args.sem = xSemaphoreCreateBinary();

    if (!args.sem) {
        goto done;
    }


    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = __test_on_ping_success;
    cbs.on_ping_timeout = __test_on_ping_timeout;
    cbs.on_ping_end = __test_on_ping_end;
    cbs.cb_args = &args;


    sprintf(args.result, "{ \"stdout\" : \"");
    VERIFY_SUCCESS_SAFERETURN(esp_ping_new_session(&ping_config, &cbs, &ping_handle),
                              goto done);

    VERIFY_SUCCESS_SAFERETURN(esp_ping_start(ping_handle), goto done);

    xSemaphoreTake(args.sem, portMAX_DELAY);

    strcat(args.result, "\"}");

done:
    if (ping_handle) {
        esp_ping_delete_session(ping_handle);
    }

    if  (args.sem) {
        vSemaphoreDelete(args.sem);
    }


    return CAN5_SUCCESS;
}

/**********************
 * MISC related
 **********************/
static CMD_FUNCTION(__reset_after, param)
{
    TRACE_FUNC;

    time_t sec = *(time_t *)param;
    ESP_LOGI(TAG, "initiating cron job - restart.");
    vTaskDelay(pdMS_TO_TICKS(sec * 1000));
    config_manager.commit_config_to_disk();
    vTaskDelay(pdMS_TO_TICKS(sec * 500));
    can5_restart();
    return CAN5_SUCCESS;
}

static CMD_FUNCTION(__enable_wifi_ap, param)
{
    TRACE_FUNC;

    VERIFY_SUCCESS(config_manager.write_bool(CFG_WIFI_AP_ENABLE, true));

    return config_manager.commit_config_to_disk();
}

static CMD_FUNCTION(__do_factory_reset, param)
{
    TRACE_FUNC;

    return config_manager.factory_default();
}

static CMD_FUNCTION(__clear_all_sensor_data, param)
{
    TRACE_FUNC;

    return can5_storage_remove_tag_fs(SENSOR_DATA_TAG);
}

static CMD_FUNCTION(__clear_old_sensor_data, param)
{
    TRACE_FUNC;

    return can5_storage_remove_old_data_fs(SENSOR_DATA_TAG);
}

static CMD_FUNCTION(__run_cb, param)
{
    TRACE_FUNC;

    return param->run_cb.run_cb(param->run_cb.run_cb_param);
}

/**********************
 * Private functions
 **********************/
static void __test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    TRACE_FUNC;

    net_ping_args_t *ping_args = args;
    // optionally, get callback arguments
    // const char* str = (const char*) args;
    // printf("%s\r\n", str); // "foo"
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    char out[64];

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    snprintf(out, 64, "%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\\n",
             recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
    ESP_LOGI(TAG, "%s", out);

    strcat(ping_args->result, out);
}

static void __test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    TRACE_FUNC;

    net_ping_args_t *ping_args = args;
    uint16_t seqno;
    ip_addr_t target_addr;
    char out[64];

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    snprintf(out, 64, "From %s icmp_seq=%d timeout\\n", inet_ntoa(target_addr.u_addr.ip4), seqno);
    ESP_LOGI(TAG, "%s", out);

    strcat(ping_args->result, out);

    xSemaphoreGive(ping_args->sem);
}

static void __test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    TRACE_FUNC;

    net_ping_args_t *ping_args = args;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    char out[64];

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    snprintf(out, 64, "%d packets transmitted, %d received, time %dms\\n", transmitted, received, total_time_ms);
    ESP_LOGI(TAG, "%s", out);

    strcat(ping_args->result, out);

    xSemaphoreGive(ping_args->sem);
}

/* ---------------------------------------------------------------------
 * Debug Support
 -----------------------------------------------------------------------*/

static const can5_tag_tab_t _cmdr_evt_tags = {
    TAG_TAB_ITEM(CAN5_CMDR_EVT_NONE),
    TAG_TAB_ITEM(CAN5_CMDR_EVT_LWAN_PAUSE),
    TAG_TAB_ITEM(CAN5_CMDR_EVT_LWAN_RESUME),
    TAG_TAB_ITEM(CAN5_CMDR_EVT_LWAN_RESET_FRAME_COUNT),
};



const char* can5_cmdr_evt_getstr(can5_cmdr_evt_t evt) {
    TRACE_FUNC;

    return TAG_LOOKUP(evt, _cmdr_evt_tags);;
}
