/**************************************************
 * Author: rmukhia
 * Creation Date: 15/11/22
 * Description: 
 **************************************************/

#include <mqtt_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include "can5_mqtt_client.h"

#include <can5_sensormng.h>
#include <can5_sensor_scd41.h>

#include "can5_error.h"
#include "can5_config.h"
#include "can5_events.h"
#include "can5_cmdr.h"
#include "can5_utils.h"
#include "can5_config_types.h"
#include "can5_hal.h"
#include "can5_cron.h"
#include "can5_rtc.h"


static const char *TAG = "CAN5_MQTT_CLIENT";
static can5_err_t __print_heap_usage(void *param);

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define MQTT_CMD_FUNCTION(name, param)  can5_err_t name(cJSON *(param))

#define READY_STR   "ready..."

typedef enum mqtt_cli_status_e {
    MQTT_CLI_STAT_UNINITD = 0,                              /**< Uninitialized */
    MQTT_CLI_STAT_INITD,                                    /**< Initialized */
    MQTT_CLI_STAT_CONNECTING,                               /**<  */
    MQTT_CLI_STAT_READY,                                    /**<  */
    MQTT_CLI_STAT_DISCONNECTED,                             /**<  */

    MQTT_CLI_STAT_LAST,
} mqtt_cli_status_t;

static struct {
    volatile mqtt_cli_status_t status;
    esp_mqtt_client_handle_t mqtt_cli;
    bool is_config_active;
    bool is_data_active;
    bool is_deveui_ready; // send deveui during lorawan mode
    int conn_retires;
    struct {
        char base[256];
    } topics;
    esp_timer_handle_t publish_stats_timer;
} __mqtt_client = {
    .status = MQTT_CLI_STAT_UNINITD,
    .mqtt_cli = NULL,
    .is_config_active = false,
    .is_data_active = false,
    .is_deveui_ready = false,
    .conn_retires = 0,
};


ESP_EVENT_DEFINE_BASE(CAN5_EVT_MQTTCLIENT);


/*************************************************************************************
 * MQTT Commands
 *************************************************************************************/
MQTT_CMD_FUNCTION(cmd_reboot, json);
MQTT_CMD_FUNCTION(cmd_upgrade, json);
MQTT_CMD_FUNCTION(cmd_set, json);
MQTT_CMD_FUNCTION(cmd_get, json);
MQTT_CMD_FUNCTION(cmd_clear_sensor_cache, json);
MQTT_CMD_FUNCTION(cmd_clear_lorawan_context, json);

MQTT_CMD_FUNCTION(cmd_get_crontab, json);
MQTT_CMD_FUNCTION(cmd_set_crontab, json);

MQTT_CMD_FUNCTION(cmd_get_rtc, json);
MQTT_CMD_FUNCTION(cmd_calibrate_scd41, json);


static struct {
    const char *cmd;
    can5_err_t (*cb)(cJSON *json);
} mqtt_cmd[] = {
    { "reboot",             cmd_reboot },
    { "upgrade",            cmd_upgrade },
    { "set",                cmd_set },
    { "get",                cmd_get },
    { "clear_sensor_cache", cmd_clear_sensor_cache },
    { "clear_lorawan_context",  cmd_clear_lorawan_context },
    { "get_crontab",        cmd_get_crontab },
    { "set_crontab",        cmd_set_crontab },
    { "get_rtc",            cmd_get_rtc },
    { "calibrate_scd41",    cmd_calibrate_scd41 }
};

#define status_ok(message)                  "{ \"status\": \"OK\", \"message\": \"" message "\" }"
#define status_error(err_str, message)      "{ \"status\" : \"" err_str "\" , \"message\": \"" message "\" }"
/*************************************************************************************
 * Forward Declarations
 *************************************************************************************/

static void __state_set(mqtt_cli_status_t next);

static void __mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

static void __hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void __net_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static can5_err_t __start_mqttclient(void *arg);

static can5_err_t __stop_mqttclient();

static char * __concat_uri(const char **str_list, size_t len);

static char *__status_msg(can5_err_t ret, const char *msg);

static can5_err_t __output(const char *data);

/*************************************************************************************
 * Module Ops
 *************************************************************************************/

can5_err_t can5_mqtt_client_init()
{
    TRACE_FUNC;
    int64_t device_id;
    int64_t project_id;
    char *organization;
    char *project;

    if (__mqtt_client.status >= MQTT_CLI_STAT_INITD) {
        return CAN5_SUCCESS;
    }

    VERIFY_SUCCESS(esp_event_handler_register(CAN5_EVT_HAL, ESP_EVENT_ANY_ID, __hal_evt_handler, NULL));

    VERIFY_SUCCESS(esp_event_handler_register(CAN5_EVT_NET, ESP_EVENT_ANY_ID, __net_evt_handler, NULL));


    VERIFY_SUCCESS(config_manager.read(CFG_DEVICE_ORGANIZATION, (uint8_t **)&organization, NULL));
    VERIFY_SUCCESS(config_manager.read_int(CFG_DEVICE_ID, &device_id));
    VERIFY_SUCCESS(config_manager.read_int(CFG_DEVICE_ID_POSTFIX, &project_id));
    VERIFY_SUCCESS(config_manager.read(CFG_PROJECT, (uint8_t  **)&project, NULL));

    snprintf(
        __mqtt_client.topics.base,
        256,
        "%s/%s/%llu%d",
        organization,
        project,
        device_id,
        (int8_t)project_id
    );

    free(organization);
    free(project);


    ESP_LOGI(TAG, "MQTT Base Topic: %s",__mqtt_client.topics.base);

    __state_set(MQTT_CLI_STAT_INITD);

    return CAN5_SUCCESS;
}

can5_err_t can5_mqtt_client_uninit()
{
    TRACE_FUNC;

    VERIFY_SUCCESS(__stop_mqttclient());

    __state_set(MQTT_CLI_STAT_UNINITD);

    return CAN5_SUCCESS;
}

int can5_mqtt_publish(const char **name_components, size_t name_components_len,
                      const char *data, size_t len, int qos, int retain)
{
    TRACE_FUNC;
    can5_err_t ret;
    char *topic;

    if (__mqtt_client.status != MQTT_CLI_STAT_READY) {
        return -1;
    }

    topic = __concat_uri(name_components, name_components_len);
    __print_heap_usage(NULL);

    if (topic) {
        /* Issue #63: Set MQTT metadata to not retain */
        // ret = esp_mqtt_client_publish(__mqtt_client.mqtt_cli, topic, data, (int)len, qos, retain);
        ret = esp_mqtt_client_publish(__mqtt_client.mqtt_cli, topic, data, (int)len, qos, false);
        ESP_LOGI(TAG, "Writing %s", topic);
        free(topic);
    }
    else {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
    }

    return ret;
}

/*************************************************************************************
 * Definition
 *************************************************************************************/

static can5_err_t __start_mqttclient(void *arg)
{
    TRACE_FUNC;

    can5_err_t ret;
    bool data_active;
    bool config_active;
    bool encryption_active;
    char *uri;
    int64_t port;
    char *username;
    char *password;
    char *device_id;
    char *tls_cert;

    uri = username = password = device_id = tls_cert = NULL;

    VERIFY_SUCCESS_SAFERETURN(
            config_manager.read_bool(CFG_MQTT_DATA_ENABLE, &data_active),
            {
                ret = r;
                goto done;
            });

    VERIFY_SUCCESS_SAFERETURN(
            config_manager.read_bool(CFG_MQTT_CONFIGURATION_ENABLE, &config_active),
            {
                ret = r;
                goto done;
            });

    VERIFY_SUCCESS_SAFERETURN(
            config_manager.read_bool(CFG_MQTT_ENCRYPTED, &encryption_active),
            {
                ret = r;
                goto done;
            });

    if (data_active || config_active) {

        VERIFY_SUCCESS_SAFERETURN(
                config_manager.read(CFG_MQTT_URI, (uint8_t **) &uri, NULL),
                {
                    ret = r;
                    goto done;
                });

        VERIFY_SUCCESS_SAFERETURN(
                config_manager.read_int(CFG_MQTT_PORT, &port),
                {
                    ret = r;
                    goto done;
                });

        VERIFY_SUCCESS_SAFERETURN(
                config_manager.read(CFG_MQTT_USERNAME, (uint8_t **) &username, NULL),
                {
                    ret = r;
                    goto done;
                });

        VERIFY_SUCCESS_SAFERETURN(
                config_manager.read(CFG_MQTT_PASSWORD, (uint8_t **) &password, NULL),
                {
                    ret = r;
                    goto done;
                });

        VERIFY_SUCCESS_SAFERETURN(
                config_manager.read(CFG_DEVICE_ID, (uint8_t **) &device_id, NULL),
                {
                    ret = r;
                    goto done;
                });
    }

    if (!config_active && !data_active) {
        /* Don't start mqtt if the user doesn't want it */
        ret = CAN5_SUCCESS;
        goto done;
    }

    __mqtt_client.is_config_active = config_active;
    __mqtt_client.is_data_active = data_active;


    esp_mqtt_client_config_t mqtt_cfg = {
      .client_id = device_id,
      .uri = uri,
      .port = port,
      .username = username,
      .password = password,
      .buffer_size = 256,
    };

    if (encryption_active) {
        size_t cert_len;
        VERIFY_SUCCESS_SAFERETURN(
                config_manager.read(CFG_MQTT_TLS_CERT, (uint8_t **) &tls_cert, &cert_len),
                {
                    ret = r;
                    goto done;
                });

        ESP_LOGI(TAG, "%s [%d]", tls_cert, cert_len);
        mqtt_cfg.cert_pem = tls_cert;
    }

    ret = CAN5_SUCCESS;

    if ((__mqtt_client.mqtt_cli = esp_mqtt_client_init(&mqtt_cfg)) == NULL) {
        goto done;
    }

    if ((ret = esp_mqtt_client_register_event(__mqtt_client.mqtt_cli,
                                   MQTT_EVENT_ANY,
                                   __mqtt_event_handler, NULL)) != CAN5_SUCCESS) {
        goto done;
    }

    ret =  esp_mqtt_client_start(__mqtt_client.mqtt_cli);

    if (ret == CAN5_SUCCESS) {
        __state_set(MQTT_CLI_STAT_CONNECTING);
    }

done:
    FREE_BULK(uri, username, password, device_id);
    return ret;
}

static can5_err_t __stop_mqttclient()
{
    TRACE_FUNC;
    esp_mqtt_client_stop(__mqtt_client.mqtt_cli);
    esp_mqtt_client_destroy(__mqtt_client.mqtt_cli);
    return CAN5_SUCCESS;
}


/* Return success status, failure is handled by lower stack */

MQTT_CMD_FUNCTION(cmd_reboot, json)
{
    TRACE_FUNC;
    can5_err_t ret;
    can5_cmd_params_t params;
    params.restart_after = 1;
    ret = can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &params, NULL, NULL);

    if (ret == CAN5_SUCCESS) {
        char *status;
        if (!(status = __status_msg(CAN5_SUCCESS, "rebooting"))) {
            return CAN5_ERR_OUT_OF_HEAP_MEMORY;
        }

        __output(status);
        free(status);

    }

    return ret;
}

MQTT_CMD_FUNCTION(cmd_clear_sensor_cache, json)
{
    TRACE_FUNC;
    can5_err_t ret;
    ret = can5_commander.add_cmd(CAN5_CMD_CLEAR_ALL_SENSOR_DATA, NULL, NULL, NULL);

    if (ret == CAN5_SUCCESS) {
        char *status;
        if (!(status = __status_msg(CAN5_SUCCESS, "cleared sensor cache data"))) {
            return CAN5_ERR_OUT_OF_HEAP_MEMORY;
        }

        __output(status);
        free(status);

    }

    return ret;
}

MQTT_CMD_FUNCTION(cmd_clear_lorawan_context, json)
{
    TRACE_FUNC;
    can5_err_t ret = CAN5_SUCCESS;
    static can5_cfg_type_t types[] = {
        CFG_LWAN_SEND_DELAY,
        CFG_LWAN_PKT_ID,
        CFG_LWAN_FW_CTX0,
        CFG_LWAN_FW_CTX1,
        CFG_LWAN_FW_CTX2,
        CFG_LWAN_FW_CTX3,
        CFG_LWAN_FW_CTX4,
        CFG_LWAN_FW_CTX5,
        CFG_LWAN_FW_CTX6,
    };

    char *empty_str = "";

    for (int i = 0; i < sizeof(types)/sizeof(types[0]); i ++) {
        ret = config_manager.write(types[i], (uint8_t *)empty_str, 0);
        if (ret != CAN5_SUCCESS) {
            break;
        }
    }

    if (ret == CAN5_SUCCESS) {
        char *status;
        if (!(status = __status_msg(CAN5_SUCCESS, "reset lorawan context"))) {
            return CAN5_ERR_OUT_OF_HEAP_MEMORY;
        }

        __output(status);
        free(status);

    }

    return ret;
}

MQTT_CMD_FUNCTION(cmd_upgrade, json)
{
    TRACE_FUNC;
    VERIFY_SUCCESS(config_manager.write_bool(CFG_OTA_MODE, true));

    char *status = NULL;
    if (!(status = __status_msg(CAN5_SUCCESS, "schedule OTA upgrade."))) {
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;
    }

    __output(status);
    free(status);

    return cmd_reboot(json);
}

MQTT_CMD_FUNCTION(cmd_set, json)
{
    TRACE_FUNC;
    cJSON *fields, *elem;
    char *status;

    /*
     * { "cmd": "set", "fields": [ {"key" : "f1", "value": "val1" }, {"key" : "f2", "value": "val2" } ] }
     */

    if (!cJSON_HasObjectItem(json, "fields")) {
        return CAN5_ERR_INVALID_PARAM;
    }

    fields = cJSON_GetObjectItem(json, "fields");

    for (can5_cfg_type_t type = 0; type < CFG_COUNT ;type++) {
        cJSON_ArrayForEach(elem, fields) {
            char *key, * value;
            const char *stype;

            if (!(cJSON_HasObjectItem(elem, "key") && cJSON_HasObjectItem(elem, "value"))){
                ESP_LOGE(TAG, "set: JSON does not have key or value.");
                continue;
            }

            key = cJSON_GetStringValue(cJSON_GetObjectItem(elem, "key"));
            value = cJSON_GetStringValue(cJSON_GetObjectItem(elem, "value"));
            stype = can5_config_getstr(type);

            if (strcmp(stype, key) == 0) {

                size_t value_len = strlen(value);

                if (config_manager.write(type, (uint8_t *)value, value_len) == CAN5_SUCCESS) {

                    const char *uris[] = {
                            "config",
                            stype
                    };

                    can5_mqtt_publish(uris, 2, value, value_len, 1, false);
                }
                break;
            }
        }
    }

    if (!(status = __status_msg(CAN5_SUCCESS, "set successful"))) {
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;
    }

    __output(status);
    free(status);

    return CAN5_SUCCESS;
}

MQTT_CMD_FUNCTION(cmd_get, json)
{
    TRACE_FUNC;
    cJSON *fields, *elem;
    char *val;
    size_t val_len;
    char *status;

    /*
     * { "cmd": "get", "fields": [ f1, f2, f3] }
     */

    if (!cJSON_HasObjectItem(json, "fields")) {
        return CAN5_ERR_INVALID_PARAM;
    }

    fields = cJSON_GetObjectItem(json, "fields");

    for (can5_cfg_type_t type = 0; type < CFG_COUNT ;type++) {
        cJSON_ArrayForEach(elem, fields) {
            if (!cJSON_IsString(elem)) {
                continue;
            }
            char *key = cJSON_GetStringValue(elem);
            const char *stype = can5_config_getstr(type);

            if (strcmp(stype, key) == 0) {
                val = NULL;
                if (config_manager.read(type, (uint8_t **)&val, &val_len) == CAN5_SUCCESS) {

                    const char *uris[] = {
                            "config",
                            stype
                    };


                    can5_mqtt_publish(uris, 2, val, val_len, 1, false);
                    free(val);
                }
                break;
            }
        }
    }

    if (!(status = __status_msg(CAN5_SUCCESS, "get successful"))) {
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;
    }

    __output(status);
    free(status);

    return CAN5_SUCCESS;
}

MQTT_CMD_FUNCTION(cmd_get_crontab, json)
{
    TRACE_FUNC;
    cJSON *root;
    char *response, *crontab;

    crontab = NULL;
    root = NULL;
    response = NULL;

    if (can5_read_crontab(&crontab) != CAN5_SUCCESS) {
        cJSON_AddStringToObject(root, "message", "Cannot retrieved crontab.");
        goto done;
    }

    root = cJSON_CreateObject();
    if (!root) {
        goto done;
    }

    cJSON_AddStringToObject(root, "crontab", crontab);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", "Retrieved crontab.");

    response = cJSON_Print(root);

done:
    if (root) {
        cJSON_Delete(root);
    }

    if (crontab) {
        free(crontab);
    }

    if (response) {
        __output(response);
        free(response);
    }

    return CAN5_SUCCESS;
}

MQTT_CMD_FUNCTION(cmd_set_crontab, json)
{
    TRACE_FUNC;
    char *crontab, *response;
    cJSON *root;
    response = NULL;

    /*
     * { "cmd": "set_crontab", "crontab": "..." }
     */

    root = cJSON_CreateObject();
    if (!root) {
        goto done;
    }

    if (!cJSON_HasObjectItem(json, "crontab")) {
        return CAN5_ERR_INVALID_PARAM;
    }

    crontab = cJSON_GetStringValue(cJSON_GetObjectItem(json, "crontab"));
    ESP_LOGI(TAG, "CORNTAB %s", crontab);

    if (can5_write_crontab(crontab) != CAN5_SUCCESS) {
        cJSON_AddStringToObject(root, "status", "error writing crontab.");
        goto done;
    }

    cJSON_AddStringToObject(root, "crontab", crontab);
    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", "Saved crontab.");
    response = cJSON_Print(root);
done:
    if (root) {
        cJSON_Delete(root);
    }

    if (response) {
        __output(response);
        free(response);
    }

    return CAN5_SUCCESS;
}

MQTT_CMD_FUNCTION(cmd_get_rtc, json)
{
    cJSON *root;
    struct tm tm;
    char status[128];
    char  *response;
    response = NULL;

    root = cJSON_CreateObject();
    if (!root) {
        goto done;
    }

    VERIFY_SUCCESS(rtc.get_time(&tm));
    time_t now = time(NULL);

    char *system_time = strdup(ctime(&now));
    char *rtc_time = strdup(asctime(&tm));

    snprintf(status, 128, "System Time: %s, RTC time: %s", system_time, rtc_time);

    free(system_time);
    free(rtc_time);

    cJSON_AddStringToObject(root, "status", "OK");
    cJSON_AddStringToObject(root, "message", status);
    response = cJSON_Print(root);

done:
    if (root) {
        cJSON_Delete(root);
    }

    if (response) {
        __output(response);
        free(response);
    }
    return CAN5_SUCCESS;
}

MQTT_CMD_FUNCTION(cmd_calibrate_scd41, json)
{
    char message[128];
    char* response = NULL;
    can5_err_t ret = CAN5_SUCCESS;

    /*
     * { "cmd": "set", "fields": [ {"key" : "f1", "value": "val1" }, {"key" : "f2", "value": "val2" } ] }
     */

    if (!cJSON_HasObjectItem(json, "target_co2")) {
        return CAN5_ERR_INVALID_PARAM;
    }

    const cJSON* target_co2 = cJSON_GetObjectItem(json, "target_co2");

    const uint16_t tco2 = (uint16_t)cJSON_GetNumberValue(target_co2);
    uint16_t status;


    // run calibration
    ret = sensor_manager.run_sensor_commands(CAN5_SENSORDRIV_TYPE_SCD41, SENSOR_SCD41_FORCE_CALIBRATE, &tco2, &status);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        goto done;
    }

    if (ret != CAN5_SUCCESS) {
        cJSON_AddStringToObject(root, "status", "error calibrating scd41.");
        snprintf(message, 128, "%s", can5_err_to_str(ret));
        cJSON_AddStringToObject(root, "message", message);
        goto done;
    }
    else
    {
        cJSON_AddStringToObject(root, "status", "calibration done.");
        snprintf(message, 128, "0x%x", status - 0x8000);
        cJSON_AddStringToObject(root, "message", &message);
    }


    response = cJSON_Print(root);

done:
    if (response) {
        __output(response);
        free(response);
    }

    if (root) {
        cJSON_Delete(root);
    }

    return CAN5_SUCCESS;
}

static can5_err_t __config_handler(void *arg)
{
    TRACE_FUNC;

    can5_mqtt_data_rx_t *data_rx = arg;
    can5_err_t ret;
    cJSON *json_req;
    char *cmd;

    ESP_LOGI(TAG, "%s", data_rx->data);

    json_req = NULL;

    if (strcmp(data_rx->data, READY_STR) == 0) {
        ret = CAN5_SUCCESS;
        goto done;
    }

    if (!(json_req = cJSON_Parse(data_rx->data))) {
        ret = CAN5_CFG_ERR_JSON;
        goto done;
    }

    if (!cJSON_HasObjectItem(json_req, "cmd")) {
        ret = CAN5_ERR_INVALID_PARAM;
        goto done;
    }

    cmd = cJSON_GetStringValue(cJSON_GetObjectItem(json_req, "cmd"));

    ret = CAN5_ERR_INVALID_PARAM;

    for (size_t i = 0; i < sizeof(mqtt_cmd)/ sizeof(mqtt_cmd[0]); i++) {
        if (strcmp(mqtt_cmd[i].cmd, cmd) == 0) {
            ret = mqtt_cmd[i].cb(json_req);
        }
    }

done:
    if (json_req) {
        cJSON_Delete(json_req);
    }

    if (data_rx->data) {
        free(data_rx->data);
    }

    free(data_rx);

    if (ret != CAN5_SUCCESS) {
        char * status = __status_msg(ret, "failure");
        if (status) {
            __output(status);
            free(status);
        }
    }

    return ret;
}

static void __handle_cmd_rx(esp_mqtt_event_handle_t event)
{
    TRACE_FUNC;
    can5_mqtt_data_rx_t *data_rx = malloc(sizeof(can5_mqtt_data_rx_t));

    if (!data_rx) {
        ESP_LOGE(TAG, "Error in allocating memory for param.");
        return;
    }

    data_rx->msg_id = event->msg_id;
    data_rx->data_len = event->data_len;
    data_rx->data = strndup(event->data, event->data_len);

    if (data_rx->data == NULL) {
        ESP_LOGE(TAG, "Error in allocating memory for event data.");
        free(data_rx);
        return;
    }

    can5_cmd_params_t run_params = {
            .run_cb = {
                    .run_cb = __config_handler,
                    .run_cb_param = data_rx,
            }
    };

    if (can5_commander.add_cmd(CAN5_CMD_RUN_CB, &run_params, NULL, NULL) != CAN5_SUCCESS) {
        free(data_rx->data);
        free(data_rx);
    }
}

static void __handle_data_rx(esp_mqtt_event_handle_t event)
{
    TRACE_FUNC;
    can5_mqtt_data_rx_t data_rx;

    data_rx.msg_id = event->msg_id;
    data_rx.data_len = event->data_len;
    data_rx.data = strndup(event->data, event->data_len);

    if (data_rx.data == NULL) {
        ESP_LOGE(TAG, "Error in allocating memory for event data.");
        return;
    }

    esp_event_post(CAN5_EVT_MQTTCLIENT, CAN5_MQTTCLIENT_EVT_DATA_ACK,
                   &data_rx, sizeof(data_rx), pdMS_TO_TICKS(1000));

}

static void __handle_rx(esp_mqtt_event_handle_t event)
{
    char *topic;
    typedef enum rx_type_e {
        NONE,
        CMD,
        DATA,
    } rx_type_t;

    rx_type_t  rx_type;

    char *uris[1];

    rx_type = NONE;

    uris[0] = "cmd";
    topic = __concat_uri((const char **) uris, 1);
    if (topic) {
        if (strncmp(event->topic, topic, event->topic_len) == 0) {
            rx_type = CMD;
        }
        free(topic);
    }

    uris[0] = "ack";
    topic = __concat_uri((const char **) uris, 1);
    if (topic) {
        if (strncmp(event->topic, topic, event->topic_len) == 0) {
            rx_type = DATA;
        }
        free(topic);
    }

    switch (rx_type) {

        case NONE:
            break;
        case CMD:
            __handle_cmd_rx(event);
            break;
        case DATA:
            __handle_data_rx(event);
            break;
    }
}

#define LORAWAN_TEMPLATE "{\"deveui\": \"%s\", \"device_name\": \"%s\" }"

static can5_err_t __publish_stats(void *params)
{
    TRACE_FUNC;
    char *val;
    char *uris[1];

    val = NULL;

    VERIFY_ALLOC_SAFENORETURN(val, 256, goto done);

    // uptime - since boot
    uris[0] = "uptime";
    sprintf(val, "%lu", can5_time(NULL));
    can5_mqtt_publish((const char **) uris, 1,
                      val, strlen(val), 0, true);


    // last-beacon - UTC
    uris[0] = "last-beacon";
    sprintf(val, "%lu", time(NULL));
    can5_mqtt_publish((const char **) uris, 1,
                      val, strlen(val), 0, true);


    // ip address
    if (hal.get_ip_sta()[0] != '\0') {
        uris[0] = "ip-addr/wifi";
        sprintf(val, "%s", hal.get_ip_sta());
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 0, true);
    }

    if (hal.get_ip_cell()[0] != '\0') {
        uris[0] = "ip-addr/cell";
        sprintf(val, "%s", hal.get_ip_cell());
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 0, true);
    }

    /* The following should be sent only once, to register lorawan deveui */
    if (__mqtt_client.is_deveui_ready) {
        char *deveui;
        char *device_name;
        uris[0] = "lorawan";

        config_manager.read(CFG_LWAN_DEVEUI, (uint8_t **)&deveui, NULL);

        config_manager.read(CFG_DEVICE_NAME, (uint8_t **)&device_name, NULL);

        char *lwan_section = malloc(256);
        sprintf(lwan_section, LORAWAN_TEMPLATE, deveui, device_name);

        ESP_LOGI(TAG, "%s", lwan_section);

        FREE_BULK(device_name, deveui);

        can5_mqtt_publish((const char **) uris, 1,
                          lwan_section, strlen(lwan_section), 0, true);
        __mqtt_client.is_deveui_ready = false;

        free(lwan_section);
    }


done:
    if (val) {
        free(val);
    }

    return CAN5_SUCCESS;
}

static void __timer_publish_stats(void* arg)
{
    can5_cmd_params_t run_cb = {
        .run_cb = {
            .run_cb = __publish_stats,
            .run_cb_param = NULL,
        }
    };

    CAN5_ERR_CHECK_NO_ABORT(can5_commander.add_cmd(CAN5_CMD_RUN_CB, &run_cb, NULL, NULL));
}

static void __handle_config_connected()
{
    TRACE_FUNC;
    char *topic, *val;
    char *uris[1];

    // id
    uris[0] = "id";
    val = NULL;
    if (config_manager.read(CFG_DEVICE_ID, (uint8_t **) &val, NULL) == CAN5_SUCCESS) {
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 1, true);
    }

    if (val) {
        free(val);
    }

    // name
    uris[0] = "name";
    val = NULL;
    if (config_manager.read(CFG_DEVICE_NAME, (uint8_t **) &val, NULL) == CAN5_SUCCESS) {
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 1, true);
    }

    if (val) {
        free(val);
    }

    // project
    uris[0] = "device_postfix";
    val = NULL;
    if (config_manager.read(CFG_DEVICE_ID_POSTFIX, (uint8_t **) &val, NULL) == CAN5_SUCCESS) {
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 1, true);
    }

    if (val) {
        free(val);
    }

    // organization
    uris[0] = "organization";
    val = NULL;
    if (config_manager.read(CFG_DEVICE_ORGANIZATION, (uint8_t **) &val, NULL) == CAN5_SUCCESS) {
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 1, true);
    }

    if (val) {
        free(val);
    }

    // app-version
    val = NULL;
    uris[0] = "app-version";
    if (config_manager.read(CFG_APP_VERSION, (uint8_t **) &val, NULL) == CAN5_SUCCESS) {
        can5_mqtt_publish((const char **) uris, 1,
                          val, strlen(val), 1, true);
    }

    if (val) {
        free(val);
    }

    uris[0] = "cmd";
    can5_mqtt_publish((const char **) uris, 1,
                      READY_STR, 8, 1, true);


    // status
    char *status = status = __status_msg(CAN5_SUCCESS, "connected");

    if (status) {
        __output(status);
        free(status);
    }

    topic = __concat_uri((const char **) uris, 1);
    assert(topic);
    if (topic) {
        esp_mqtt_client_subscribe(__mqtt_client.mqtt_cli, topic, 1);
        free(topic);
    }


    const esp_timer_create_args_t publish_stats_timer_args = {
        .callback = &__timer_publish_stats,
        .name = "mqtt publish stats",
        .skip_unhandled_events = true,
    };

    CAN5_ERR_CHECK(esp_timer_create(&publish_stats_timer_args, &__mqtt_client.publish_stats_timer));

    __publish_stats(NULL);

    esp_timer_start_periodic(__mqtt_client.publish_stats_timer, CONFIG_CAN5_MQTT_PUBLISH_STATS_INTERVAL_MS * 1000);
}

static void __handle_data_connected()
{
    TRACE_FUNC;
    char *topic;
    char *uris[1];

    uris[0] = "ack";
    topic = __concat_uri((const char **) uris, 1);
    assert(topic);
    esp_mqtt_client_subscribe(__mqtt_client.mqtt_cli, topic, 1);
    free(topic);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void __mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    TRACE_FUNC;

    esp_mqtt_event_handle_t event = event_data;
    // esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            __state_set(MQTT_CLI_STAT_READY);

            esp_event_post(CAN5_EVT_MQTTCLIENT, CAN5_MQTTCLIENT_EVT_CONNECTED,
                           NULL, 0, 100);

            if (__mqtt_client.is_config_active) {
                __handle_config_connected();
            }

            if (__mqtt_client.is_data_active) {
                __handle_data_connected();
            }

            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

            __state_set(MQTT_CLI_STAT_DISCONNECTED);

            esp_event_post(CAN5_EVT_MQTTCLIENT, CAN5_MQTTCLIENT_EVT_DISCONNECTED,
                           NULL, 0, 100);

            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);

            if (__mqtt_client.is_config_active || __mqtt_client.is_data_active) {
                __handle_rx(event);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static void __hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    TRACE_FUNC;


    if (__mqtt_client.status == MQTT_CLI_STAT_UNINITD) return;

    switch (event_id) {

        case CAN5_HAL_EVT_WIFI_STA_CONNECTED:
        case CAN5_HAL_EVT_CELL_CONNECTED:
            if (__mqtt_client.status == MQTT_CLI_STAT_INITD) {
                ESP_LOGI(TAG, "[ >> Start MQTTClient ]");

                // aggressively add this command
                can5_cmd_params_t run_cb = {
                    .run_cb = {
                        .run_cb = __start_mqttclient,
                        .run_cb_param = NULL,
                    }
                };

                while(can5_commander.add_cmd(CAN5_CMD_RUN_CB, &run_cb, NULL, NULL) != CAN5_SUCCESS) {
                    vTaskDelay(1);
                }
            }
            break;
        default:
            break;
    }
}

static void __net_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    TRACE_FUNC;

    switch (event_id) {
        case CAN5_NET_EVT_LORAWAN_GOT_DEVEUI:
            ESP_LOGI(TAG, "[ >> Got deveui ]");
            __mqtt_client.is_deveui_ready = true;
            break;
        default:
            break;
    }

}

static void __state_set(mqtt_cli_status_t next)
{
    TRACE_FUNC;

    __mqtt_client.status = next;
}

static char * __concat_uri(const char **str_list, size_t len)
{
    char *topic = NULL;
    size_t mem_len;

    mem_len = strlen(__mqtt_client.topics.base);

    for (int i = 0; i < len; i++) {
        mem_len += 1; // seperator '/'
        mem_len += strlen(str_list[i]);
    }
    mem_len += 1; // end char

    topic = malloc(mem_len);
    if (!topic) {
        return NULL;
    }

    strcpy(topic, __mqtt_client.topics.base);
    for (int i = 0; i < len ; i++) {
        strcat(topic, "/");
        strcat(topic, str_list[i]);
    }

    return topic;
}

static char *__status_msg(can5_err_t ret, const char *msg)
{
    char *result;
    size_t len;
    const char *fmt = "{ \"status\": \"%s\", \"message\": \"%s\" }";
    len = snprintf(NULL, 0, fmt, can5_err_to_str(ret), msg) + 1;
    result = malloc(len);
    snprintf(result, len, fmt, can5_err_to_str(ret), msg);
    return result;
}

static can5_err_t __output(const char *data)
{
    const char *uris[] = {
            "out"
    };

    if (can5_mqtt_publish(uris, 1,
                data, strlen(data), 0, false) == -1) {
        return CAN5_NET_ERR_NIC_TIMEOUT;
    }

    return CAN5_SUCCESS;
}

/* ---------------------------------------------------------------------
 * Debug Support
 -----------------------------------------------------------------------*/

static const can5_tag_tab_t _mqttclient_evt_tags = {
    TAG_TAB_ITEM(CAN5_MQTTCLIENT_EVT_NONE),
    TAG_TAB_ITEM(CAN5_MQTTCLIENT_EVT_CONNECTED),
    TAG_TAB_ITEM(CAN5_MQTTCLIENT_EVT_DISCONNECTED),
    TAG_TAB_ITEM(CAN5_MQTTCLIENT_EVT_DATA_ACK),
};


const char* can5_mqttclient_evt_getstr(can5_mqttclient_evt_t evt) {
    TRACE_FUNC;

    return TAG_LOOKUP(evt, _mqttclient_evt_tags);;
}

#define LEAK_NOTIFY_THRESHOLD       0
static can5_err_t __print_heap_usage(void *param)
{

    static size_t last_size = 0;
    size_t new_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

    if (abs((int)last_size - (int)new_size) >= LEAK_NOTIFY_THRESHOLD) {
        ESP_LOGI(TAG, "Free Mem (Check Possible Mem Leak): %d" , new_size);
        last_size = new_size;
    }

    return CAN5_SUCCESS;
}
