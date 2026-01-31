/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   25/02/22
*
* File:  can5_httpserver.c
* Descr:
*******************************************************************************/

#include "can5_httpserver.h"
#include <string.h>
#include <sys/param.h>
#include <can5_http_json.h>
#include <ping/ping_sock.h>
#include <can5_utils.h>
#include <freertos/event_groups.h>
#include <can5_netmng.h>
#include <esp_tls_crypto.h>
#include "can5_hal.h"
#include "can5_config.h"
#include "can5_rtc.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "can5_sensormng.h"
#include "can5_cmdr.h"
#include "can5_cron.h"
#include "can5_storagedriv.h"

static const char *TAG = "HTTPSERVER";
#define AUTH_USERNAME    "canariner"
#define AUTH_REALM       "Basic realm=\"canarin5\""

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif


static const char * status_ok = "{ \"status\": \"OK\" }";
__attribute__((unused)) static const char * status_error = "{ \"status\": \"ERROR\" }";

static can5_err_t init();
static can5_err_t uninit();

can5_httpserver_t httpserver = {
  .module = {
      .init = init,
      .uninit = uninit,
  },
};

typedef enum httpserver_status_e {
  HTTPSERV_UNINITD,
  HTTPSERV_INITD,
  HTTPSERV_SERVING,
} httpserver_status_t;

typedef enum events_evt_grp_e {
    EVENT_WIFI_SCAN_SUCCESS =   0,
    EVENT_WIFI_SCAN_ERROR,
    EVENT_OTA_UPGRADE_SUCCESS,
    EVENT_OTA_UPGRADE_ERROR,
    EVENT_NET_PING_SUCCESS,
    EVENT_NET_PING_ERROR,
    EVENT_REMOVE_SENSOR_DATA_SUCCESS,
    EVENT_REMOVE_SENSOR_DATA_ERROR,
} events_evt_grp_t;

static struct can5_httpserver_hdl_s {
    httpserver_status_t status;
    httpd_handle_t server;
    struct {
        esp_ping_handle_t hdl;
        char buf[512];
    } ping;

    struct {
        bool init;
        char *digest;
    } basic_auth;
    EventGroupHandle_t *evt_grp;

} __httpserver = {
    .status = HTTPSERV_UNINITD,
    .server = NULL,
    .ping = {
        .hdl = NULL,
    },
    .basic_auth = {
        .init = false,
        .digest = NULL,
    },
};




/*************************************************************************************
 * Forward Declarations
 *************************************************************************************/

static void __state_set(httpserver_status_t next);

static void __hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static can5_err_t __start_webserver(void *arg);

static can5_err_t __stop_webserver();

/*************************************************************************************
 * HTTP URLs
 *************************************************************************************/

static esp_err_t get_handler(httpd_req_t *req);

static esp_err_t post_handler(httpd_req_t *req);

typedef enum can5_uri_e {
    CAN5_STATIC_URI_FAVICION = 0,
    CAN5_STATIC_URI_INDEX,
    CAN5_STATIC_URI_MAIN_CSS,
    CAN5_STATIC_URI_MAIN_JS,
    CAN5_STATIC_URI_MANIFEST_JSON,
    CAN5_STATIC_URI_MAIN_JS_LICENSE,
    CAN5_STATIC_URI_FIELDS_JSON,

    CAN5_API_URI_STATUS,
    CAN5_API_URI_GENERAL,
    CAN5_API_URI_COMMUNICATION,
    CAN5_API_URI_WIFI_AP,
    CAN5_API_URI_WIFI_STA,
    CAN5_API_URI_CELLULAR,
    CAN5_API_URI_LORAWAN,
    CAN5_API_URI_SENSORS,
    CAN5_API_URI_CRONTAB,
    CAN5_API_URI_LOGGING,

    CAN5_API_URI_COMMAND,

    CAN5_URI_MAX,
} can5_uri_t;

static const httpd_uri_t app_uri[] = {
    /* Static url handlers */
    {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_FAVICION,
    },
    {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_INDEX,
    },
    {
        .uri       = "/device",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_INDEX,
    },
    {
        .uri       = "/networks",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_INDEX,
    },
    {
        .uri       = "/sensors",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_INDEX,
    },
    {
        .uri       = "/crontab",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_INDEX,
    },
    {
        .uri       = "/logging",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_INDEX,
    },
    {
        .uri       = "/static/css/main.css",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_MAIN_CSS,
    },
    {
        .uri       = "/static/js/main.js",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_MAIN_JS,
    },
    {
        .uri       = "/api/fields",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_FIELDS_JSON,
    },
    {
        .uri       = "/manifest.json",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_MANIFEST_JSON,
    },
    {
        .uri       = "/static/js/main.js.LICENSE.txt",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_STATIC_URI_MAIN_JS_LICENSE,
    },

    /* Rest url handlers */
    {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_STATUS,
    },
    {
        .uri       = "/api/general",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_GENERAL,
    },
    {
        .uri       = "/api/general",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_GENERAL,
    },
    {
        .uri       = "/api/communication",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_COMMUNICATION,
    },
    {
        .uri       = "/api/communication",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_COMMUNICATION,
    },
    {
        .uri       = "/api/wifi_ap",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_WIFI_AP,
    },
    {
        .uri       = "/api/wifi_ap",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_WIFI_AP,
    },
    {
        .uri       = "/api/wifi_sta",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_WIFI_STA,
    },
    {
        .uri       = "/api/wifi_sta",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_WIFI_STA,
    },
    {
        .uri       = "/api/cellular",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_CELLULAR,
    },
    {
        .uri       = "/api/cellular",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_CELLULAR,
    },
    {
        .uri       = "/api/lorawan",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_LORAWAN,
    },
    {
        .uri       = "/api/lorawan",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_LORAWAN,
    },
    {
        .uri       = "/api/sensors",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_SENSORS,
    },
    {
        .uri       = "/api/sensors",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_SENSORS,
    },
    {
        .uri       = "/api/crontab",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_CRONTAB,
    },
    {
        .uri       = "/api/crontab",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_CRONTAB,
    },
    {
        .uri       = "/api/logging",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = (void *) CAN5_API_URI_LOGGING,
    },
    {
        .uri       = "/api/logging",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_LOGGING,
    },
    {
        .uri       = "/api",
        .method    = HTTP_POST,
        .handler   = post_handler,
        .user_ctx  = (void *) CAN5_API_URI_COMMAND,
    },
};


/*************************************************************************************
 * Static GET request handlers
 *************************************************************************************/

static esp_err_t __favicon_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char favicon_start[] asm("_binary_favicon_ico_gz_start");
    extern const unsigned char favicon_end[] asm("_binary_favicon_ico_gz_end");
    const size_t favicon_ico_size = (favicon_end - favicon_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *) favicon_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t __index_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char index_start[] asm("_binary_index_html_gz_start");
    extern const unsigned char index_end[] asm("_binary_index_html_gz_end");
    const size_t content_size = (index_end - index_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *) index_start, content_size);

    return ESP_OK;
}

static esp_err_t __main_css_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char main_css_start[] asm("_binary_main_css_gz_start");
    extern const unsigned char main_css_end[] asm("_binary_main_css_gz_end");
    const size_t content_size = (main_css_end - main_css_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *) main_css_start, content_size);

    return ESP_OK;
}

static esp_err_t __main_js_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char main_js_start[] asm("_binary_main_js_gz_start");
    extern const unsigned char main_js_end[] asm("_binary_main_js_gz_end");
    const size_t content_size = (main_js_end - main_js_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (const char *) main_js_start, content_size);

    return ESP_OK;
}

static esp_err_t __fields_json_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char fields_json_start[] asm("_binary_fields_json_start");
    extern const unsigned char fields_json_end[] asm("_binary_fields_json_end");
    const size_t content_size = (fields_json_end - fields_json_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *) fields_json_start, content_size);

    return ESP_OK;
}

static esp_err_t __manifest_json_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char manifest_json_start[] asm("_binary_manifest_json_gz_start");
    extern const unsigned char manifest_json_end[] asm("_binary_manifest_json_gz_end");
    const size_t content_size = (manifest_json_end - manifest_json_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *) manifest_json_start, content_size);

    return ESP_OK;
}

static esp_err_t __main_js_license_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    extern const unsigned char main_js_license_start[] asm("_binary_main_js_LICENSE_txt_gz_start");
    extern const unsigned char main_js_license_end[] asm("_binary_main_js_LICENSE_txt_gz_end");
    const size_t content_size = (main_js_license_end - main_js_license_start);
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (const char *) main_js_license_start, content_size);

    return ESP_OK;
}

/*************************************************************************************
 * API GET request handlers
 *************************************************************************************/

static esp_err_t __api_status_get_handler(httpd_req_t *req)
{
    TRACE_FUNC;
    cJSON *res_root, *elem;
    char *response;
    can5_err_t ret;

    can5_sensor_meta_details_t sensor_details[CAN5_IO_PORT_MAX];
    size_t sensor_details_len;

    static const char *json_template = "{\
        \"battery_percentage\": 33,\
        \"charging\": true\
    }";

    res_root = cJSON_Parse(json_template);
    char *app_version = NULL;

    if (!res_root) {
        ret = CAN5_CFG_ERR_JSON;
        goto done;
    }

    if (sensor_manager.get_ready_sensors(sensor_details, &sensor_details_len) == CAN5_SUCCESS) {
        cJSON *sensor_arr = cJSON_CreateArray();
        for (size_t i = 0; i < sensor_details_len; i ++) {
            cJSON *sensor_elem = cJSON_CreateObject();
            cJSON_AddStringToObject(sensor_elem, "name", sensor_details[i].name);
            cJSON_AddStringToObject(sensor_elem, "manufacturer", sensor_details[i].manufacturer);
            cJSON_AddStringToObject(sensor_elem, "version", sensor_details[i].version);
            cJSON_AddStringToObject(sensor_elem, "last_reading", sensor_details[i].last_reading);
            cJSON_AddStringToObject(sensor_elem, "type", (char *)can5_sensor_type_getstr(sensor_details[i].type));
            cJSON_AddStringToObject(sensor_elem, "port", (char *) can5_hal_port_getstr(sensor_details[i].port));
            if (sensor_details[i].serial_num) {
                cJSON_AddStringToObject(sensor_elem, "serial_number", sensor_details[i].serial_num);
                free(sensor_details[i].serial_num);
            }
            cJSON_AddItemToArray(sensor_arr, sensor_elem);
        }
        if (sensor_arr) {
            cJSON_AddItemToObject(res_root, "online_sensors", sensor_arr);
        }

    }

    int64_t uptime = can5_time(NULL);

    elem = cJSON_AddNumberToObject(res_root, "uptime_sec", (double)uptime);
    if (!elem) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    int64_t system_time = time(NULL);
    if (system_time == -1) {
    	return CAN5_TIME_ERR_GET_SYS_TIME;
    	goto done;
    }

    elem = cJSON_AddNumberToObject(res_root, "system_time", (double)system_time);
    if (!elem) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }
    //int64_t num_data = 0;

    //if (config_manager.get_counter("data", &num_data) == CAN5_SUCCESS) {
    //    elem = cJSON_AddNumberToObject(res_root, "num_data", (double)num_data);
    //    if (!elem) {
    //        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
    //        goto done;
    //    }
    //}

    can5_netmng_connected_protos_t connected_protos[CAN5_NETPROTO_COUNT];
    CLEAR_ARRAY(connected_protos);
    size_t netproto_len;
    netproto_len = net.is_connected((can5_netmng_connected_protos_t  *)&connected_protos);

    elem = cJSON_AddArrayToObject(res_root, "connected");
    if (!elem) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    for(size_t i = 0; i < netproto_len; i++) {
        cJSON *nelem = cJSON_CreateObject();
        if (!nelem) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }
        if (!cJSON_AddStringToObject(nelem, "proto", connected_protos[i].proto_name)) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }

        if (!cJSON_AddBoolToObject(nelem, "connected", connected_protos[i].is_connected)) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }

        if (connected_protos[i].status_str && !cJSON_AddStringToObject(nelem, "status", connected_protos[i].status_str)) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }

        if (!cJSON_AddItemToArray(elem, nelem)) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }
    }


    if ((ret = config_manager.read(CFG_APP_VERSION, (uint8_t **)&app_version, NULL)) == CAN5_SUCCESS) {
        elem = cJSON_AddStringToObject(res_root, "app_version", app_version);
        if (!elem) {
            ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
            goto done;
        }
    }

    response = cJSON_Print(res_root);
    if (!response) {
        cJSON_Delete(res_root);
        ret =  CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    free(response);
    ret = CAN5_SUCCESS;

done:
    if (app_version) {
        free(app_version);
    }
    cJSON_Delete(res_root);
    return ret;
}

static can5_err_t __api_get_from_config(httpd_req_t *req, can5_http_json_type_t type)
{
    TRACE_FUNC;
    char *json;

    json = can5_http_json_str_get(type);
    if (!json) {
        return CAN5_CFG_ERR_JSON;
    }

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);

    free(json);

    return CAN5_SUCCESS;

}

static can5_err_t __api_post_to_config(httpd_req_t *req, can5_http_json_type_t type, char *content)
{
    TRACE_FUNC;
    char *res;
    res = can5_http_json_set(type, content);

    TRACE_FUNC;
    if (!res) {
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;
    }

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, res, HTTPD_RESP_USE_STRLEN);

    free(res);

    return CAN5_SUCCESS;
}


static can5_err_t __get_str_from_json(const char *key, cJSON *root, char *out_str)
{
    can5_err_t ret;
    cJSON *cmd_elem;
    char *elem_str;

    ret = CAN5_SUCCESS;

    cmd_elem = NULL;
    elem_str = NULL;

    if (!cJSON_HasObjectItem(root, key)) {
        ret = CAN5_CFG_ERR_JSON;
        goto done;
    }

    if ((cmd_elem = cJSON_GetObjectItem(root, key)) == NULL) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    if ((elem_str = cJSON_GetStringValue(cmd_elem)) == NULL) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    strcpy(out_str, elem_str);

done:
    return ret;
}

/*************************************************************************************
 * POST Common
 *************************************************************************************/

#define POST_COMMON_FUNCTION(name, param)  static can5_err_t name(httpd_req_t *req, cJSON *param)


POST_COMMON_FUNCTION(cmd_get, json);
POST_COMMON_FUNCTION(cmd_set, json);
POST_COMMON_FUNCTION(cmd_online_sensors, json);
POST_COMMON_FUNCTION(cmd_internet_ping, json);
POST_COMMON_FUNCTION(cmd_wifi_scan, json);
POST_COMMON_FUNCTION(cmd_reboot, json);
POST_COMMON_FUNCTION(cmd_reset_lorawan_context, json);
POST_COMMON_FUNCTION(cmd_remove_wifi_ssid, json);
POST_COMMON_FUNCTION(cmd_firmware_upgrade, json);
POST_COMMON_FUNCTION(cmd_update_time, json);
POST_COMMON_FUNCTION(cmd_remove_sensor_data, json);
POST_COMMON_FUNCTION(cmd_get_cron_jobs, json);
POST_COMMON_FUNCTION(cmd_get_status, json);


static struct {
    const char *cmd;
    can5_err_t (*cb)(httpd_req_t *req, cJSON *json);
} common_cmd[] = {
        {
                "get", cmd_get
        },
        {
                "set", cmd_set
        },
        {
                "online_sensors", cmd_online_sensors
        },
        {
                "internet_ping", cmd_internet_ping
        },
        {
                "wifi_scan", cmd_wifi_scan
        },
        {
                "reboot", cmd_reboot
        },
        {
                "reset_lorawan_context", cmd_reset_lorawan_context
        },
        {
                "remove_wifi_ssid", cmd_remove_wifi_ssid
        },
        {
                "firmware_upgrade", cmd_firmware_upgrade
        },
        {
                "update_time", cmd_update_time
        },
        {
                "remove_sensor_data", cmd_remove_sensor_data
        },
        {
                "get_cron_jobs", cmd_get_cron_jobs
        },
        {
            "get_status", cmd_get_status
        }


};

POST_COMMON_FUNCTION(cmd_get, json)
{
    TRACE_FUNC;

    cJSON *fields, *elem;
    cJSON *reply_json;
    char *reply;

    /*
     * { "cmd": "get", "fields": [ f1, f2, f3] }
     */

    if (!cJSON_HasObjectItem(json, "fields")) {
        return CAN5_ERR_INVALID_PARAM;
    }

    fields = cJSON_GetObjectItem(json, "fields");

    reply_json = cJSON_CreateObject();

    cJSON_ArrayForEach(elem, fields) {
        if (!cJSON_IsString(elem)) {
            continue;
        }
        char *key = cJSON_GetStringValue(elem);

        for (can5_cfg_type_t type = 0; type < CFG_COUNT ;type++) {
            const char *s_type = can5_config_getstr(type);
            if (strcmp(s_type, key) == 0) {
                char *val = NULL;
                if (config_manager.read(type, (uint8_t **)&val, NULL) == CAN5_SUCCESS) {
                    if (val) {
                        cJSON_AddStringToObject(reply_json, key, val);
                    }
                    free(val);
                }
                break;
            }
        }
    }

    reply = cJSON_PrintUnformatted(reply_json);
    cJSON_Delete(reply_json);

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, reply, HTTPD_RESP_USE_STRLEN);

    free(reply);

    return CAN5_SUCCESS;

}

static const char *__none__ = "";

POST_COMMON_FUNCTION(cmd_set, json)
{
    TRACE_FUNC;
    cJSON *fields, *elem;
    cJSON  *reply_json;
    char *reply;

    /*
     * { "cmd": "set", "fields": [ {"key" : "f1", "value": "val1" }, {"key" : "f2", "value": "val2" } ] }
     */

    if (!cJSON_HasObjectItem(json, "fields")) {
        return CAN5_ERR_INVALID_PARAM;
    }

    fields = cJSON_GetObjectItem(json, "fields");

    reply_json = cJSON_CreateObject();

    cJSON_ArrayForEach(elem, fields) {
        const char *key, *value;
        if (!(cJSON_HasObjectItem(elem, "key") && cJSON_HasObjectItem(elem, "value"))){
            ESP_LOGE(TAG, "set: JSON does not have key or value.");
            cJSON_AddStringToObject(reply_json, "ERROR", "JSON does not have key or value.");
            break;
        }

        key = cJSON_GetStringValue(cJSON_GetObjectItem(elem, "key"));
        value = cJSON_GetStringValue(cJSON_GetObjectItem(elem, "value"));

        if (!(cJSON_IsString(cJSON_GetObjectItem(elem, "key")) && cJSON_IsString(cJSON_GetObjectItem(elem, "value")))){
            ESP_LOGE(TAG, "set: JSON is not string.");
            cJSON_AddStringToObject(reply_json, "ERROR", "All key and value must be string.");
            break;
        }

        for (can5_cfg_type_t type = 0; type < CFG_COUNT ;type++) {
            const char *s_type = can5_config_getstr(type);

            if (strcmp(s_type, key) == 0) {
                char err_key[CAN5_STORAGE_KEY_LEN_MAX + 8];
                CLEAR_ARRAY(err_key);
                snprintf(err_key, CAN5_STORAGE_KEY_LEN_MAX + 8, "%s_STATUS", key);

                bool failed = false;
                bool skip;
                if (can5_validate_param(type, value, &skip)) {
                    const char *val;
                    if (!strcmp(value, "__NONE__")) {
                        val = __none__;
                    }
                    else {
                        val = value;
                    }

                    if (!skip) {
                        if (config_manager.write(type, (uint8_t *)val, strlen(val)) != CAN5_SUCCESS) {
                            failed = true;
                            cJSON_AddStringToObject(reply_json, err_key, "SAVE_ERROR");
                        }
                    }
                }
                else {
                    failed = true;
                    cJSON_AddStringToObject(reply_json, err_key, "PARAM_INVALID");
                }

                if (!failed) {
                    cJSON_AddStringToObject(reply_json, err_key, "SUCCESS");
                }

                cJSON_AddStringToObject(reply_json, key, value);
                break;
            }
        }
    }

    reply = cJSON_PrintUnformatted(reply_json);
    cJSON_Delete(reply_json);

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, reply, HTTPD_RESP_USE_STRLEN);

    free(reply);

    return CAN5_SUCCESS;
}

POST_COMMON_FUNCTION(cmd_online_sensors, json)
{
    TRACE_FUNC;
    cJSON  *reply_json;
    char *reply;

    can5_sensor_meta_details_t sensor_details[CAN5_IO_PORT_MAX];
    size_t sensor_details_len;

    reply_json = cJSON_CreateArray();

    if (sensor_manager.get_ready_sensors(sensor_details, &sensor_details_len) == CAN5_SUCCESS) {
        for (size_t i = 0; i < sensor_details_len; i ++) {
            cJSON *sensor_elem = cJSON_CreateObject();
            cJSON_AddStringToObject(sensor_elem, "name", sensor_details[i].name);
            cJSON_AddStringToObject(sensor_elem, "manufacturer", sensor_details[i].manufacturer);
            cJSON_AddStringToObject(sensor_elem, "version", sensor_details[i].version);
            cJSON_AddStringToObject(sensor_elem, "last_reading", sensor_details[i].last_reading);
            cJSON_AddStringToObject(sensor_elem, "type", (char *)can5_sensor_type_getstr(sensor_details[i].type));
            cJSON_AddStringToObject(sensor_elem, "port", (char *) can5_hal_port_getstr(sensor_details[i].port));
            if (sensor_details[i].serial_num) {
                cJSON_AddStringToObject(sensor_elem, "serial_number", sensor_details[i].serial_num);
                free(sensor_details[i].serial_num);
            }
            cJSON_AddItemToArray(reply_json, sensor_elem);
        }
    }

    reply = cJSON_PrintUnformatted(reply_json);
    cJSON_Delete(reply_json);

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, reply, HTTPD_RESP_USE_STRLEN);

    free(reply);

    return CAN5_SUCCESS;
}

static void __ping_complete(can5_err_t ret, can5_cmd_params_t *params, void *user_data)
{
    TRACE_FUNC;
    httpd_req_t *req = user_data;

    if (ret == CAN5_SUCCESS) {
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, params->ping_ip.result);
        xEventGroupSetBits(__httpserver.evt_grp, BIT(EVENT_NET_PING_SUCCESS));
    }
    else {
        xEventGroupSetBits(__httpserver.evt_grp, BIT(EVENT_NET_PING_ERROR));

    }
}

POST_COMMON_FUNCTION(cmd_internet_ping, json)
{
    TRACE_FUNC;

    can5_cmd_params_t params;

    CLEAR_ARRAY(params.ping_ip.address);
    VERIFY_SUCCESS(__get_str_from_json("address", json, (char *)params.ping_ip.address));

    VERIFY_ALLOC(params.ping_ip.result, 512);

    VERIFY_SUCCESS_SAFERETURN(can5_commander.add_cmd(CAN5_CMD_PING_IP, &params, __ping_complete, req),
                              free(params.ping_ip.result));

    EventBits_t bits = xEventGroupWaitBits(
            __httpserver.evt_grp,
            BIT(EVENT_NET_PING_SUCCESS) | BIT(EVENT_NET_PING_ERROR),
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(20000) ); // 20 seconds


    free(params.ping_ip.result);

    if (bits & BIT(EVENT_NET_PING_SUCCESS)) {
        return CAN5_SUCCESS;
    }
    else if (bits & BIT(EVENT_NET_PING_ERROR)) {
        return CAN5_ERROR;
    }

    return CAN5_SUCCESS;
}

static void __wifi_scan_complete(can5_err_t ret, can5_cmd_params_t *result_params, void *user_data)
{
    httpd_req_t *req = user_data;
    wifi_ap_record_t *ap_info;
    cJSON *res_root;
    char *response;
    bool error;

    res_root = NULL;
    response = NULL;
    ap_info = NULL;
    error = false;

    if (ret != CAN5_SUCCESS) {
        error = true;
        goto done;
    }

    res_root = cJSON_CreateArray();
    if (!res_root) {
        error = true;
        goto done;
    }

    ap_info = result_params->wifi_scan.ap_info;

    for (int i = 0; (i < CONFIG_CAN5_HTTPSERVER_WIFI_SCAN_MAX) && (i < result_params->wifi_scan.ap_count); i++) {
        cJSON *elem;

        elem = cJSON_CreateObject();
        if  (!elem) {
            httpd_resp_send_500(req);
            goto done;
        }

        cJSON_AddStringToObject(elem, "ssid", (const char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(elem, "rssi", ap_info[i].rssi);

        cJSON_AddItemToArray(res_root, elem);
        ESP_LOGI_V(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI_V(TAG, "RSSI \t\t%d", ap_info[i].rssi);
    }

    response = cJSON_Print(res_root);
    if (!response) {
        error = true;
        goto done;
    }


    done:
    if (!error) {
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, response);
        xEventGroupSetBits(__httpserver.evt_grp, BIT(EVENT_WIFI_SCAN_SUCCESS));
    }
    else {
        xEventGroupSetBits(__httpserver.evt_grp, BIT(EVENT_WIFI_SCAN_ERROR));
    }

    if (response) free(response);
    if (res_root) cJSON_Delete(res_root);
}

POST_COMMON_FUNCTION(cmd_wifi_scan, json)
{
    TRACE_FUNC;
    can5_cmd_params_t params;

    params.wifi_scan.ap_info = NULL;

    params.wifi_scan.max_scan = CONFIG_CAN5_HTTPSERVER_WIFI_SCAN_MAX;
    params.wifi_scan.ap_count = 0;
    params.wifi_scan.ap_info = calloc(sizeof(wifi_ap_record_t), params.wifi_scan.max_scan);

    if (!params.wifi_scan.ap_info) {
        return CAN5_ERR_OUT_OF_HEAP_MEMORY;
    }

    VERIFY_SUCCESS_SAFERETURN(can5_commander.add_cmd(CAN5_CMD_SCAN_WIFI, &params, __wifi_scan_complete, req),
                              free(params.wifi_scan.ap_info));

    EventBits_t bits = xEventGroupWaitBits(
            __httpserver.evt_grp,
            BIT(EVENT_WIFI_SCAN_SUCCESS) | BIT(EVENT_WIFI_SCAN_ERROR),
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(20000)); // 20 seonds

    free(params.wifi_scan.ap_info);

    if (bits & BIT(EVENT_WIFI_SCAN_SUCCESS)) {
        return CAN5_SUCCESS;
    }
    else if (bits & BIT(EVENT_WIFI_SCAN_ERROR)) {
        return CAN5_ERROR;
    }

    ESP_LOGE(TAG, "Wifi scan has invalid bit set.");
    return CAN5_ERROR;
}

POST_COMMON_FUNCTION(cmd_reboot, json)
{
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, status_ok);
    can5_cmd_params_t params;
    params.restart_after = 1;
    return can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &params, NULL, NULL);
}

POST_COMMON_FUNCTION(cmd_reset_lorawan_context, json)
{
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
        config_manager.write(types[i], (uint8_t *)empty_str, 0);
    }

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, status_ok);

    // reboot automatically to avoid the current context being written
    can5_cmd_params_t params;
    params.restart_after = 1;
    return can5_commander.add_cmd(CAN5_CMD_RESET_AFTER, &params, NULL, NULL);

}

POST_COMMON_FUNCTION(cmd_remove_wifi_ssid, json)
{
    static can5_cfg_type_t types[] = {
            CFG_WIFI_STA_SSID,
            CFG_WIFI_STA_PASS,
    };

    char *empty_str = "";

    for (int i = 0; i < sizeof(types)/sizeof(types[0]); i ++) {
        config_manager.write(types[i], (uint8_t *)empty_str, 0);
    }

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, status_ok);
    return CAN5_SUCCESS;

}

POST_COMMON_FUNCTION(cmd_firmware_upgrade, json)
{
    TRACE_FUNC;

    VERIFY_SUCCESS(config_manager.write_bool(CFG_OTA_MODE, true));

    return cmd_reboot(req, json);
}

POST_COMMON_FUNCTION(cmd_update_time, json)
{
    TRACE_FUNC;
    char systime[32];
    char result[128];
    CLEAR_ARRAY(systime);
    struct tm tm;
    time_t sec;

    VERIFY_SUCCESS(__get_str_from_json("system_time", json, systime));

    sec = strtol(systime, NULL, 10);

    tm = *gmtime(&sec);

    VERIFY_SUCCESS(rtc.set_time(&tm));
    VERIFY_SUCCESS(rtc.rtc_to_sys());

    CLEAR_ARRAY(result);

    snprintf(result, 128, "{ \"status\": \"ok\", \"system_time\": %ld }", sec);
    httpd_resp_sendstr(req, result);
    return CAN5_SUCCESS;
}


static void __remove_sensor_data_complete(can5_err_t ret, can5_cmd_params_t *result, void *user_data)
{
    httpd_req_t *req = user_data;

    if (ret == CAN5_SUCCESS) {
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, status_ok);
        xEventGroupSetBits(__httpserver.evt_grp, BIT(EVENT_REMOVE_SENSOR_DATA_SUCCESS));
    }
    else {
        xEventGroupSetBits(__httpserver.evt_grp, BIT(EVENT_REMOVE_SENSOR_DATA_ERROR));
    }

}

POST_COMMON_FUNCTION(cmd_remove_sensor_data, json)
{
    TRACE_FUNC;
    VERIFY_SUCCESS(can5_commander.add_cmd(CAN5_CMD_CLEAR_ALL_SENSOR_DATA, NULL, __remove_sensor_data_complete, req));
    EventBits_t bits = xEventGroupWaitBits(__httpserver.evt_grp,
                                           BIT(EVENT_REMOVE_SENSOR_DATA_SUCCESS) | BIT(EVENT_REMOVE_SENSOR_DATA_ERROR),
                                           pdTRUE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & BIT(EVENT_REMOVE_SENSOR_DATA_SUCCESS)) {
        return CAN5_SUCCESS;
    }
    else if (bits & BIT(EVENT_REMOVE_SENSOR_DATA_ERROR)) {
        return CAN5_ERROR;
    }

    ESP_LOGE(TAG, "Remove sensor data has invalid bit set.");
    return CAN5_ERROR;
}


POST_COMMON_FUNCTION(cmd_get_cron_jobs, json)
{
    TRACE_FUNC;
    static char *jobs[24];
    char *response;
    static size_t len;
    cJSON *result, *res_root;
    can5_err_t res;

    res = CAN5_SUCCESS;
    res_root = result = NULL;
    response = NULL;

    can5_cron_get_jobs((const char **)jobs, &len);

    res_root = cJSON_CreateObject();

    if (!res_root) {
        res = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    if (!cJSON_AddStringToObject(res_root, "status", "ok")) {
        res = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }


    result = cJSON_CreateStringArray((const char * const *)jobs, (int) len);

    if (!result) {
        res = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    if (!cJSON_AddItemToObject(res_root, "cron_jobs", result)) {
        res = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        cJSON_Delete(result);
        goto done;
    }

    response = cJSON_Print(res_root);

    if (!response) {
        res = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    done:
    if (response) {
        free(response);
    }

    if (res_root) {
        cJSON_Delete(res_root);
    }

    return res;
}


POST_COMMON_FUNCTION(cmd_get_status, json)
{
    TRACE_FUNC;
    char *response;
    char *val;
    cJSON *reply_json;
    can5_err_t res;

    uint8_t sta_mac[6];
    char *mac_str;

    res = CAN5_SUCCESS;
    response = NULL;
    reply_json = cJSON_CreateObject();


    can5_netmng_connected_protos_t connected_protos[CAN5_NETPROTO_COUNT];
    CLEAR_ARRAY(connected_protos);
    size_t netproto_len;
    netproto_len = net.is_connected((can5_netmng_connected_protos_t  *)&connected_protos);

    cJSON *array_elem = cJSON_AddArrayToObject(reply_json, "connected");


    for(size_t i = 0; i < netproto_len; i++) {
        cJSON *nelem = cJSON_CreateObject();
        cJSON_AddStringToObject(nelem, "proto", connected_protos[i].proto_name);
        cJSON_AddBoolToObject(nelem, "connected", connected_protos[i].is_connected);
        cJSON_AddStringToObject(nelem, "status", connected_protos[i].status_str);
        cJSON_AddItemToArray(array_elem, nelem);
    }

    val = NULL;
    if ((res = config_manager.read(CFG_DEVICE_NAME, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_DEVICE_NAME", val);
        free(val);
    }

    val = NULL;
    if ((res = config_manager.read(CFG_DEVICE_ID, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_DEVICE_ID", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_DEVICE_ID_POSTFIX, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_DEVICE_ID_POSTFIX", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_APP_VERSION, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_APP_VERSION", val);
        free(val);
    }
    int64_t uptime = can5_time(NULL);
    int64_t system_time = time(NULL);

    cJSON_AddNumberToObject(reply_json, "uptime_sec", (double)uptime);
    cJSON_AddNumberToObject(reply_json, "system_time", (double)system_time);

    val = NULL;
    if ((res = config_manager.read(CFG_LAST_G_ALT, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_LAST_G_ALT", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_LAST_G_LAT, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_LAST_G_LAT", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_LAST_G_LNG, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_LAST_G_LNG", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_LAST_G_TIME, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_LAST_G_TIME", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_MQTT_DATA_ENABLE, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_MQTT_DATA_ENABLE", val);
        free(val);
    }
    val = NULL;
    if ((res = config_manager.read(CFG_LORARELAY_ENABLE, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
        cJSON_AddStringToObject(reply_json, "CFG_MQTT_LORARELAY_ENABLE", val);
        free(val);
    }

#ifndef CAN5_LINUX_HOST_TEST
    // the following is only for  WIFI STA
    cJSON *wifi_json = cJSON_CreateObject();
    cJSON_AddItemToObject(reply_json, "wifi", wifi_json);

    hal.get_mac_addresses(NULL, sta_mac);

    mac_str = __get_formatted_mac_address(sta_mac);
    cJSON_AddStringToObject(wifi_json, "mac", mac_str);

    wifi_ap_record_t ap_info;
    bool wifi_connected;
    hal.get_wifi_sta_status(&ap_info, &wifi_connected);
    cJSON_AddBoolToObject(wifi_json, "connected", wifi_connected);
    if (wifi_connected) {
        cJSON_AddStringToObject(wifi_json, "ssid", (char *)ap_info.ssid);
        mac_str = __get_formatted_mac_address(ap_info.bssid);
        cJSON_AddStringToObject(wifi_json, "bssid", mac_str);
        cJSON_AddNumberToObject(wifi_json, "rssi", ap_info.rssi);
        cJSON_AddNumberToObject(wifi_json, "channel", ap_info.primary);
        cJSON_AddStringToObject(wifi_json, "ip", hal.get_ip_sta());
    }

    // cellular
    cJSON *cellular_json = cJSON_CreateObject();
    cJSON_AddItemToObject(reply_json, "cellular", cellular_json);
    const char *cell_ip = hal.get_ip_cell();
    if (strlen(cell_ip) != 0) {
        cJSON_AddBoolToObject(cellular_json, "connected", true);
        cJSON_AddStringToObject(cellular_json, "ip", cell_ip);
        cJSON_AddNumberToObject(cellular_json, "rssi", hal.get_cell_rssi());

        val = NULL;
        if ((res = config_manager.read(CFG_CELL_APN, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
            cJSON_AddStringToObject(cellular_json, "CFG_CELL_APN", val);
            free(val);
        }

        val = NULL;
        if ((res = config_manager.read(CFG_CELL_OPERATOR, (uint8_t **)&val, NULL)) == CAN5_SUCCESS) {
            cJSON_AddStringToObject(cellular_json, "CFG_CELL_OPERATOR", val);
            free(val);
        }
    }
    else {
        cJSON_AddBoolToObject(cellular_json, "connected", false);
    }

    cJSON *lora_json = cJSON_CreateObject();
    cJSON_AddItemToObject(reply_json, "lora", lora_json);
    bool lora_enabled = false;
    config_manager.read_bool(CFG_LWAN_ENABLE, &lora_enabled);

    cJSON_AddBoolToObject(lora_json, "enabled", lora_enabled);
    if (lora_enabled) {
        int64_t lora_rssi, lora_snr;
        config_manager.read_int(CFG_LWAN_STATS_SNR, &lora_snr);
        cJSON_AddNumberToObject(lora_json, "CFG_LWAN_STATS_SNR", (double)lora_snr);
        config_manager.read_int(CFG_LWAN_STATS_RSSI, &lora_rssi);
        cJSON_AddNumberToObject(lora_json, "CFG_LWAN_STATS_RSSI", (double)lora_rssi);
    }


#endif

    response = cJSON_PrintUnformatted(reply_json);
    cJSON_Delete(reply_json);

    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    free(response);

    return CAN5_SUCCESS;



}

static can5_err_t __api_post_common(httpd_req_t *req, char *content)
{
    TRACE_FUNC;

    can5_err_t ret;
    cJSON *root;
    char *cmd;

    if (!(root = cJSON_Parse(content))) {
        ret = CAN5_CFG_ERR_JSON;
        goto done;
    }

    if (!cJSON_HasObjectItem(root, "cmd")) {
        ret = CAN5_ERR_INVALID_PARAM;
        goto done;
    }

    cmd = cJSON_GetStringValue(cJSON_GetObjectItem(root, "cmd"));

    ret = CAN5_ERR_INVALID_PARAM;

    for (size_t i = 0; i < sizeof(common_cmd)/ sizeof(common_cmd[0]); i++) {
        if (strcmp(common_cmd[i].cmd, cmd) == 0) {
            ESP_LOGI(TAG, "Found command %s", cmd);
            ret = common_cmd[i].cb(req, root);
        }
    }

done:
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}

/*************************************************************************************
 * Request URL dispatchers
 *************************************************************************************/

static can5_err_t get_url_dispatcher(httpd_req_t *req)
{
    TRACE_FUNC;

    can5_uri_t uri_type = (can5_uri_t) req->user_ctx;

    /* return okay on invalid url */
    can5_err_t ret = CAN5_SUCCESS;
    switch (uri_type) {
        case CAN5_STATIC_URI_FAVICION:
            ret = __favicon_get_handler(req);
            break;
        case CAN5_STATIC_URI_INDEX:
            ret = __index_get_handler(req);
            break;
        case CAN5_STATIC_URI_MAIN_CSS:
            ret = __main_css_get_handler(req);
            break;
        case CAN5_STATIC_URI_MAIN_JS:
            ret = __main_js_get_handler(req);
            break;
        case CAN5_STATIC_URI_FIELDS_JSON:
            ret = __fields_json_get_handler(req);
            break;
        case CAN5_STATIC_URI_MAIN_JS_LICENSE:
            ret = __main_js_license_get_handler(req);
            break;
        case CAN5_STATIC_URI_MANIFEST_JSON:
            ret = __manifest_json_get_handler(req);
            break;
        case CAN5_API_URI_STATUS:
            ret = __api_status_get_handler(req);
            break;
        case CAN5_API_URI_GENERAL:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_GENERAL);
            break;
        case CAN5_API_URI_COMMUNICATION:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_COMMUNICATION);
            break;
        case CAN5_API_URI_WIFI_AP:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_WIFI_AP);
            break;
        case CAN5_API_URI_WIFI_STA:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_WIFI_STA);
            break;
        case CAN5_API_URI_CELLULAR:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_CELLULAR);
            break;
        case CAN5_API_URI_LORAWAN:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_LORAWAN);
            break;
        case CAN5_API_URI_SENSORS:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_IO);
            break;
        case CAN5_API_URI_CRONTAB:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_CRONTAB);
            break;
        case CAN5_API_URI_LOGGING:
            ret = __api_get_from_config(req, CAN5_HTTP_JSON_LOGGING);
            break;

        default:
            break;
    }
    return ret;
}

static can5_err_t post_url_dispatcher(httpd_req_t *req, char *content)
{
    TRACE_FUNC;

    can5_uri_t uri_type = (can5_uri_t) req->user_ctx;

    /* return okay on invalid url */
    esp_err_t ret = CAN5_SUCCESS;
    switch (uri_type) {
        case CAN5_API_URI_GENERAL:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_GENERAL, content);
            break;
        case CAN5_API_URI_COMMUNICATION:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_COMMUNICATION, content);
            break;
        case CAN5_API_URI_WIFI_AP:
            // the password changes here
            __httpserver.basic_auth.init = false;
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_WIFI_AP, content);
            break;
        case CAN5_API_URI_WIFI_STA:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_WIFI_STA, content);
            break;
        case CAN5_API_URI_CELLULAR:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_CELLULAR, content);
            break;
        case CAN5_API_URI_LORAWAN:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_LORAWAN, content);
            break;
        case CAN5_API_URI_SENSORS:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_IO, content);
            break;
        case CAN5_API_URI_LOGGING:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_LOGGING, content);
            break;
        case CAN5_API_URI_CRONTAB:
            ret = __api_post_to_config(req, CAN5_HTTP_JSON_CRONTAB, content);
            break;
        case CAN5_API_URI_COMMAND:
            ret = __api_post_common(req, content);
            break;
        default:
            break;
    }
    config_manager.commit_config_to_disk();
    return ret;
}

/*************************************************************************************
 * Http method handlers
 *************************************************************************************/

static can5_err_t calculate_basic_auth_digest()
{
    char *val, *digest;
    size_t n, digest_len;

    val = NULL;
    n = 0;

    if (__httpserver.basic_auth.init) {
        return CAN5_SUCCESS;
    }

    if (__httpserver.basic_auth.digest) {
        free(__httpserver.basic_auth.digest);
    }

    // 10['canariner:'] + 32
    VERIFY_ALLOC(val, 42);

    strcpy(val, AUTH_USERNAME":");

    char *pass = NULL;
    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_WIFI_AP_PASS, (uint8_t  **)&pass, NULL),
                              free(val));
    strcat(val, pass);
    free(pass);

    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)val, strlen(val));

    // Basic (6) + n + end(1) length
    VERIFY_ALLOC_SAFERETURN(digest, 6 + n + 1, free(val));

    strcpy(digest, "Basic ");
    esp_crypto_base64_encode((unsigned char *)digest + 6, n, &digest_len, (const unsigned char *)val, strlen(val));

    free(val);
    __httpserver.basic_auth.digest = digest;
    __httpserver.basic_auth.init = true;

    return CAN5_SUCCESS;
}

static can5_err_t verify_basic_auth(httpd_req_t *req)
{
    size_t buf_len;
    char *buf = NULL;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI_V(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
            goto auth_error;
        }

        if(strncmp(__httpserver.basic_auth.digest, buf, buf_len) != 0) {
            goto auth_error;
        }
    }
    else {
        goto auth_error;
    }

    if (buf) free(buf);
    return CAN5_SUCCESS;

auth_error:
    if (buf) free(buf);
    ESP_LOGE(TAG, "Not authenticated");
    httpd_resp_set_status(req, "401 UNAUTHORIZED");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "WWW-Authenticate", AUTH_REALM);
    httpd_resp_send(req, NULL, 0);
    return CAN5_HTTP_ERR_AUTH;
}

static esp_err_t get_handler(httpd_req_t *req)
{
    TRACE_FUNC;

    ESP_LOGI(TAG, "GET: %s", req->uri);

    can5_err_t ret;
    /* Calculate basic auth digest if not already calculated. */
    if ((ret = calculate_basic_auth_digest()) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK_NO_ABORT(ret);
        httpd_resp_send_500(req);
        return ESP_ERR_HTTPD_INVALID_REQ;
    }

    /* verify auth */
    if ((ret = verify_basic_auth(req)) != CAN5_SUCCESS) {
        /* if not successful just return */
        CAN5_ERR_CHECK_NO_ABORT(ret);
        return ESP_ERR_HTTPD_INVALID_REQ;
    }

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    if ((ret = get_url_dispatcher(req)) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK_NO_ABORT(ret);
        httpd_resp_send_500(req);
        return ESP_ERR_HTTPD_INVALID_REQ;
    }

    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char *content;
    esp_err_t ret;

    /* Truncate if content length larger than the buffer */
    size_t recv_size = req->content_len;

    ESP_LOGI(TAG, "POST: %s", req->uri);

    content = NULL;
    content = calloc(1, recv_size + 1);

    int len = httpd_req_recv(req, content, recv_size);
    if (len <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (len == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        ret =  ESP_FAIL;
        goto done;
    }

    can5_err_t can5_ret;
    /* Calculate basic auth digest if not already calculated. */
    if ((can5_ret = calculate_basic_auth_digest()) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK_NO_ABORT(can5_ret);
        httpd_resp_send_500(req);
        ret =  ESP_ERR_HTTPD_INVALID_REQ;
        goto done;
    }

    /* verify auth */
    if ((can5_ret = verify_basic_auth(req)) != CAN5_SUCCESS) {
        /* if not successful just return */
        CAN5_ERR_CHECK_NO_ABORT(can5_ret);
        ret = ESP_ERR_HTTPD_INVALID_REQ;
        goto done;
    }

    if ((can5_ret = post_url_dispatcher(req, content)) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK_NO_ABORT(can5_ret);
        if (can5_ret == CAN5_ERR_INVALID_PARAM) {
            httpd_resp_send_404(req);
        }
        else {
            httpd_resp_send_500(req);
        }
        ret = ESP_ERR_HTTPD_INVALID_REQ;
        goto done;
    }

    ret = ESP_OK;

done:
    if (content) {
        free(content);
    }

    return ret;
}


/*************************************************************************************
 * Module Ops
 *************************************************************************************/

static can5_err_t init()
{
    TRACE_FUNC;

    if (__httpserver.status >= HTTPSERV_INITD) {
        return CAN5_SUCCESS;
    }

    __httpserver.evt_grp = xEventGroupCreate();
    assert(__httpserver.evt_grp); // assume its always created

    VERIFY_SUCCESS(esp_event_handler_register(CAN5_EVT_HAL, ESP_EVENT_ANY_ID, __hal_evt_handler, NULL));

    __state_set(HTTPSERV_INITD);

    return CAN5_SUCCESS;
}

static can5_err_t uninit()
{
    TRACE_FUNC;

    VERIFY_SUCCESS(__stop_webserver());

    __state_set(HTTPSERV_UNINITD);

    return CAN5_SUCCESS;
}

static can5_err_t __start_webserver(void *arg)
{
    TRACE_FUNC;

    //char sntp_server[32];

    if (__httpserver.status == HTTPSERV_UNINITD) {
        return CAN5_ERR_INVALID_STATE;
    }

    if (__httpserver.status == HTTPSERV_SERVING) {
        return CAN5_SUCCESS;
    }

    //can5_err_t ret;


    __httpserver.server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 18;
    size_t num_uri = sizeof(app_uri) / sizeof(app_uri[0]);
    config.max_uri_handlers = num_uri + 1;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&__httpserver.server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        for (size_t i = 0; i < num_uri; i++) {
            httpd_register_uri_handler(__httpserver.server, &app_uri[i]);
        }

        __state_set(HTTPSERV_SERVING);

        return CAN5_SUCCESS;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return CAN5_ERROR;
}

static can5_err_t __stop_webserver()
{
    TRACE_FUNC;

    // Stop the httpd server
    httpd_stop(__httpserver.server);

    return CAN5_SUCCESS;
}


/*************************************************************************************
 * Private Definition
 *************************************************************************************/
static void __hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    TRACE_FUNC;


    if (__httpserver.status == HTTPSERV_UNINITD) return;

    switch (event_id) {
        case CAN5_HAL_EVT_WIFI_AP_START:
        case CAN5_HAL_EVT_WIFI_STA_CONNECTED:
            ESP_LOGI(TAG, "[>> Start HTTPServer ]");

            // aggressively add this command
            can5_cmd_params_t run_cb = {
                .run_cb = {
                    .run_cb = __start_webserver,
                    .run_cb_param = NULL,
                }
            };

            while(can5_commander.add_cmd(CAN5_CMD_RUN_CB, &run_cb, NULL, NULL) != CAN5_SUCCESS) {
                vTaskDelay(1);
            }
            break;
        default:
            break;
    }
}

static void __state_set(httpserver_status_t next)
{
    __httpserver.status = next;
}
