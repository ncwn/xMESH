/**
 * @file   can5_netif_wifi.c
 * @author Raunak Mukhia (@rmukhia)
 * @brief 
 * @version 0.1
 * @date    2021-02-10
 * 
 */

//_______________________________________________________________________________________________________
//
//   DEFINES
//-------------------------------------------------------------------------------------------------------

#include <can5_events.h>
#include "can5_netif_wrapper.h"
#include "can5_hal.h"
#include "can5_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "can5_config_provider.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/sockets.h"

/*
 * TODO: this module assumes that wifi is connected after this module is initialzied.
 */

static const char *TAG = "NETIF_WRAPPER";
#define WPPR_TASK_STACK_SIZE                2540

#define CHECK_INITD() do {                       \
    if (__netif_wppr.status == NETIF_WPPR_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define CONNECTION_EVT(evt_type) {              \
    if (__netif_wppr.conncb) {                  \
        can5_net_connect_evt_t evt = {          \
            .type = (evt_type),                 \
            .netif_id = get_id(),               \
        };                                      \
        __netif_wppr.conncb(&evt);              \
    }                                           \
}
//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------


/**
 * @brief Netif status
 * 
 */
typedef enum netif_wprstatus_e {
    NETIF_WPPR_STAT_UNINITD = 0,                              /**< Uninitialized */
    NETIF_WPPR_STAT_INITD,                                    /**< Initialized */
    NETIF_WPPR_STAT_CONNECTING,                               /**<  */
    NETIF_WPPR_STAT_DISCONNECTED,                                /**<  */
    NETIF_WPPR_STAT_SEND_READY,                               /**<  */

    NETIF_WPPR_STAT_LAST,
} netif_wpr_status_t;

typedef enum netif_wpr_evt_grp_e {
    NETIF_WIFI_EVT_GRP_WIFI_CONN_FAIL = BIT0,
    NETIF_WIFI_EVT_GRP_GOT_IP = BIT1,
} netif_wpr_evt_grp_t;

//_______________________________________________________________________________________________________
//
//   INTERNAL FUNCTIONS DECLARATION
//-------------------------------------------------------------------------------------------------------

static uint8_t get_id();

static void set_id(uint8_t id);

// return 0 if detected, and save details in the specified location
static can5_err_t detect(can5_port_idx_t port);

// initialize the module to be ready to connect
static can5_err_t init(can5_port_idx_t port);

// run in main loop
static can5_err_t run();

// send bytes through the interface
static can5_err_t wsend(const void *data, size_t len);

// initialize the module to be ready to connect
static can5_err_t uninit();

static can5_err_t wrecv(void *prxdata, size_t *len, uint16_t timeout);

// set the callback function to execute upon reception of a packet on a specific port
static can5_err_t register_recv_cb(can5_net_rxcb_f *);

// return true if connection is considered established
static bool is_connected(void);

// establish a connection with the server
static can5_err_t wconnect(can5_net_conncb_f *conncb, bool wait);

// run extra commands
static can5_err_t driverctl(uint8_t request, void *params, void *response);

// return rssi if available in dbm
static can5_netif_rssi_t rssi_get(void);

// Get driver status
static int32_t status_get();

static void __hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief Return a string corresponding to the status descruption
 * 
 * @return const char* status description 
 */
static const char *status_getstr(int32_t status);

#endif

//_______________________________________________________________________________________________________
//
//   VARIABLES
//-------------------------------------------------------------------------------------------------------


/**
 * @brief Module available functions
 * 
 */
const can5_netif_t netif_wppr = {
    .ops = {
        .get_id = get_id,
        .set_id = set_id,
        .detect = detect,
        .send = wsend,
        .init = init,
        .uninit = uninit,
        .run = run,
        .recv = wrecv,
        .register_recv_cb = register_recv_cb,
        .is_connected = is_connected,
        .connect = wconnect,
        .driverctl = driverctl,
        .rssi_get = rssi_get,
        .status_get = status_get,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0
        .status_getstr = status_getstr,
#endif
    },
    .details = {
        .type = CAN5_NET_TYPE_LWIP,
        .name = "Can5 NetIf Template",
        .version = "1.0",
        .manufacturer = "AIT intERLab",
    },
    .is_sleepable = false,
};

#define STA_MODE    (1 << 0)
#define AP_MODE     (1 << 1)
#define APSTA_MODE  (AP_MODE | STA_MODE)

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
    StackType_t stack[WPPR_TASK_STACK_SIZE];
} task_t;

static struct netif_wpr_hdl_s {
    volatile netif_wpr_status_t status;
    uint8_t id;
    can5_port_idx_t detected;
    can5_net_rxcb_f *rxcb;
    can5_net_conncb_f *conncb;
    struct sockaddr_in dest_addr; // destination address
    int sock; // udp socket
    EventGroupHandle_t event_group;
} __netif_wppr = {
    .status = NETIF_WPPR_STAT_UNINITD,
    .detected = CAN5_PORT_NULL,
    .rxcb = NULL,
    .conncb = NULL,
    .sock = 0,
    .event_group = NULL,
};


/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
static void __state_set(netif_wpr_status_t next);

static can5_err_t __disconnect(void *params, void *response);


//_______________________________________________________________________________________________________
//
//   FUNCTIONS DEFINITION
//-------------------------------------------------------------------------------------------------------
static uint8_t get_id()
{
    return __netif_wppr.id;
}

static void set_id(uint8_t id)
{
    __netif_wppr.id = id;
}

can5_err_t detect(can5_port_idx_t port)
{
    TRACE_FUNC;

    /* WI-FI should only respond to NETPORT_0 */
    if (port != NETPORT_0) {
        return CAN5_NET_ERR_INVALID_DRIVER;
    }

    __netif_wppr.detected = port;

    return CAN5_SUCCESS;
}


can5_err_t init(can5_port_idx_t port)
{
    TRACE_FUNC;


    if (__netif_wppr.detected != port) {
        VERIFY_SUCCESS(detect(port));   // verify that the module is present in the specified port
    }

    if (__netif_wppr.status >= NETIF_WPPR_STAT_INITD) {
        return CAN5_SUCCESS;  // do no re-init
    }

    VERIFY_SUCCESS(esp_event_handler_register(CAN5_EVT_HAL, ESP_EVENT_ANY_ID, __hal_evt_handler, (void*)&__netif_wppr));

    __state_set(NETIF_WPPR_STAT_INITD);

    return CAN5_SUCCESS;
}

// initialize the module to be ready to connect
can5_err_t uninit()
{
    TRACE_FUNC;

    if (__netif_wppr.status == NETIF_WPPR_STAT_UNINITD) return CAN5_SUCCESS;

    if (__netif_wppr.status == NETIF_WPPR_STAT_SEND_READY) {
        CAN5_ERR_CHECK(__disconnect(NULL, NULL));
    }

    __state_set(NETIF_WPPR_STAT_UNINITD);

    return CAN5_SUCCESS;
}

// establish a connection with the server
static can5_err_t wconnect(can5_net_conncb_f *conncb, bool wait)
{
    TRACE_FUNC;

    CHECK_INITD();

    if (is_connected()) {
        __netif_wppr.conncb = conncb;
        __state_set(NETIF_WPPR_STAT_SEND_READY);
        return CAN5_SUCCESS;
    }


    // TODO: wait implementation

    __netif_wppr.conncb = conncb;

    __state_set(NETIF_WPPR_STAT_CONNECTING);

    return CAN5_SUCCESS;
}


static can5_err_t run()
{
    TRACE_FUNC;
    return CAN5_SUCCESS;
}

// send bytes through the interface
can5_err_t wsend(const void *data, size_t len)
{
    TRACE_FUNC;
    ESP_LOGE(TAG, "%s unimplemented", __func__);
    return CAN5_SUCCESS;
}


can5_err_t wrecv(void *prxdata, size_t *len, uint16_t timeout)
{
    TRACE_FUNC;
    CHECK_INITD();
    ESP_LOGE(TAG, "%s unimplemented", __func__);

    return CAN5_SUCCESS;
}

// set the callback function to execute upon reception of a packet on a specific port
can5_err_t register_recv_cb(can5_net_rxcb_f *cb)
{
    TRACE_FUNC;

    VERIFY_NOT_NULL(cb);
    __netif_wppr.rxcb = cb;

    return CAN5_SUCCESS;
}

// return true if connection is considered established
bool is_connected(void)
{
    TRACE_FUNC;

    return __netif_wppr.status == NETIF_WPPR_STAT_SEND_READY;
}

static char status_str[32];

static can5_err_t driverctl(uint8_t request, void *params, void *response)
{
    TRACE_FUNC;

    can5_err_t ret = CAN5_SUCCESS;

    switch (request) {
        case CAN5_DRIVERCTL_NETIF_WPPR_CONN_STATUS:
        {
            *(char **)response = status_str;
            CLEAR_ARRAY(status_str);
#if 0
            int16_t wifi_rssi, cell_rssi;
            size_t len;
            wifi_rssi = hal.get_wifi_rssi();
            cell_rssi = hal.get_cell_rssi();
            len = 0;
            if (wifi_rssi != 0) {
                len += sprintf(&status_str[len], "Wifi Rssi: %d", wifi_rssi);
            }

            if (cell_rssi != 0) {
                sprintf(&status_str[len], "Cell Rssi: %d", cell_rssi);
            }
#endif
            time_t last_rx;
            config_manager.read_int(CFG_MQTT_STATS_LAST_ACK, (int64_t *)&last_rx);
            snprintf(status_str, 64, "Last_Rx: %ld", last_rx);
        }
        break;
        default:
            ret = CAN5_ERR_INVALID_DRIVERCTL;
    }
    return ret;
}

// return rssi if available in dbm
can5_netif_rssi_t rssi_get(void)
{
    TRACE_FUNC;

    if (__netif_wppr.status == NETIF_WPPR_STAT_UNINITD) return CAN5_RSSI_UNAVAILABLE;

    return 0;
}

static int32_t status_get()
{
    return __netif_wppr.status;
}


/* ---------------------------------------------------------------------
 * Private functions
 -----------------------------------------------------------------------*/

static void __hal_evt_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{

    struct netif_wpr_hdl_s *___wifi;
    //static can5_net_connect_evt_t evt;
    VERIFY_NOT_NULL_VOID(arg);
    ___wifi = arg;


    if (___wifi->status == NETIF_WPPR_STAT_UNINITD) return;

    switch (event_id) {

        case CAN5_HAL_EVT_WIFI_STA_CONNECTED:
        case CAN5_HAL_EVT_CELL_CONNECTED:
            ESP_LOGI(TAG, "Net is connected.");
            __state_set(NETIF_WPPR_STAT_SEND_READY);
            CONNECTION_EVT(CAN5_NET_CONNEVT_CONNECTED);
            break;
        case CAN5_HAL_EVT_WIFI_STA_DISCONNECTED:
        case CAN5_HAL_EVT_CELL_DISCONNECTED:
            ESP_LOGI(TAG, "Net is disconnected.");
            __state_set(NETIF_WPPR_STAT_DISCONNECTED);
            CONNECTION_EVT(CAN5_NET_CONNEVT_DISCONNECTED);
            break;

    }
}


static void __state_set(netif_wpr_status_t next)
{
    __netif_wppr.status = next;
}


static can5_err_t __disconnect(void *params, void *response)
{
    /* unimplimented */

    return CAN5_SUCCESS;
}



//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief NEtif Status tags
 * 
 */
static const can5_tag_tab_t _NETIF_WPPR_STAT_tags = {
    TAG_TAB_ITEM(NETIF_WPPR_STAT_UNINITD),
    TAG_TAB_ITEM(NETIF_WPPR_STAT_CONNECTING),
    TAG_TAB_ITEM(NETIF_WPPR_STAT_DISCONNECTED),
    TAG_TAB_ITEM(NETIF_WPPR_STAT_SEND_READY),
};


static const char *status_getstr(int32_t status)
{
    return TAG_LOOKUP(status, _NETIF_WPPR_STAT_tags);
}

#endif

