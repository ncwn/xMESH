#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "can5_netmng.h"
#include "can5_events.h"
#include "can5_wiring.h"
#include "can5_utils.h"
#include "esp_log.h"
#include "can5_config.h"
#include "can5_storagemng.h"
//#include "can5_netif_sim7600.h"
#include "can5_hal.h"
#include "can5_netif_wrapper.h"
#include "can5_netif_lwan.h"
#include "can5_netproto.h"
#include "can5_sensor_data.h"


//_______________________________________________________________________________________________________
//
//   Defines
//-------------------------------------------------------------------------------------------------------
#define TAG "NETMNG"

#define CAN5_NETIF_DET_STRLEN 100

#define DEFAULT_NET_ACK_WAIT_TIMEOUT                5 //seconds
#define STORAGE_STACK_MAX_SEARCH_DEPTH              32 // this  defines the max records to search in the stack.
#define MAX_ACK_WAIT_TICK                           pdMS_TO_TICKS(30000)


#define NETIF_LIST_FIND_ID(id, netif_inst)   {                  \
    netif_inst_t *cur;                                          \
    netif_inst = NULL;                                          \
    TAILQ_FOREACH(cur, &__net.netif_list, te)  {                \
        if (cur->netif->ops.get_id() == id) {                   \
            netif_inst = cur;                                   \
            break;                                              \
        }                                                       \
    }                                                           \
}

#define NETPROTO_LIST_FIND_ID(id, netproto_inst)   {            \
    netproto_inst_t *cur;                                       \
    netproto_inst = NULL;                                       \
    TAILQ_FOREACH(cur, &__net.netproto_list, te)  {             \
        if (cur->netproto->get_id() == id) {                \
            netproto_inst = cur;                                \
            break;                                              \
        }                                                       \
    }                                                           \
}

#define NETPROTO_FOREACH_NETIF(netproto_inst, netif_inst, stmts) TAILQ_FOREACH(netproto_inst,   \
                                                                    &__net.netproto_list, te) { \
if (netproto_inst->netif == netif_inst) stmts;                                                  \
}

//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------

typedef enum can5_netmng_state_e {
    NETMNG_STATE_UNINITD = 0,  /**< Uninitialized */
    NETMNG_STATE_INITD,        /**< Initialized */
    NETMNG_STATE_LAST,
} can5_netmng_state_t;


typedef enum netproto_status_e {
    NETPROTO_STAT_DISCONNECTED = 0,
    NETPROTO_STAT_CONNECTING,
    NETPROTO_STAT_CONNECTED,
    NETPROTO_STAT_READY,
    NETPROTO_STAT_RUN,
    NETPROTO_STAT_RUNNING,
    NETPROTO_STAT_COMPLETED,
    NETPROTO_STAT_FAILED,

} netproto_status_t;


//_______________________________________________________________________________________________________
//
//   INTERNAL FUNCTIONS DECLARATION
//-------------------------------------------------------------------------------------------------------

/**
 * @brief initialize the module and its submodules
 * 
 * @return CAN5_SUCCESS if ok, otherwise error 
 */
static can5_err_t init();

/**
 * @brief Uninit the module  and its submodules
 * 
 * @return CAN5_SUCCESS if ok, otherwise error 
 */
static can5_err_t uninit();

static can5_err_t pre_sleep();

static can5_err_t post_sleep();

static bool is_sleepable();

static bool is_sleeping();

/**
 * @brief get the module status code
 * 
 */
static int32_t status_get();

static size_t is_connected(can5_netmng_connected_protos_t *connected_protos);

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief Return a string corresponding to the status description
 * 
 * @return const char* status description 
 */
static const char *status_getstr(int32_t status);

/**
 * @brief Return a string corresponding to the event description
 * 
 * @return const char* event description 
 */

static const char *evt_getstr(int32_t status);

/**
 * @brief Return a string corresponding to the netproto status description
 *
 * @return const char* netproto status description
 */
__attribute__((unused)) static const char *status_netproto_getstr(netproto_status_t status);

#endif

/**
 * @brief Return the name of the interffcae type
 * 
 * @param dt 
 * @return const char* 
 */
__attribute__((unused)) static const char *_netif_details_getstr(const can5_netif_details_t *dt);


static can5_err_t run();

static bool is_active();

//_______________________________________________________________________________________________________
//
//   VARIABLES
//-------------------------------------------------------------------------------------------------------


const can5_netmng_t net = {
    .module = {
        .init = init,
        .uninit = uninit,
        .pre_sleep = pre_sleep,
        .post_sleep = post_sleep,
        .is_sleepable = is_sleepable,
        .is_sleeping = is_sleeping,
        .status_get = status_get,
        .run = run,
        .is_active = is_active,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0
        .status_getstr = status_getstr,
        .evt_getstr = evt_getstr,
#endif
    },
    .is_connected = is_connected,
};

typedef struct buf_s {
    uint8_t buf[CAN5_STORAGE_MAX_LEN];
    size_t len;
} buf_t;

typedef enum task_notif_e {
    TASK_NOTIF_NETIF_COMPLETED = 0,
    TASK_NOTIF_NETIF_FAILED,
    TASK_NOTIF_NETIF_INVALID_DATA,
    TASK_NOTIF_NEW_DATA,
} task_notif_t;

typedef struct tx_q_msg_s {
    uint32_t id;
    const uint8_t *data;
    size_t len;
} tx_q_msg_t;

typedef struct netif_inst_s {
    can5_netif_t const *netif;
    can5_port_idx_t port;
    bool connected;
    TAILQ_ENTRY(netif_inst_s) te;
} netif_inst_t;

typedef TAILQ_HEAD(netif_inst_head_s, netif_inst_s) netif_inst_head_t;

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
} task_t;

static struct {
    StackType_t netmng[CONFIG_CAN5_NETWORKMNG_TASK_STACK_SIZE];
    StackType_t proto[CAN5_NETPROTO_COUNT][CONFIG_CAN5_NETWORKMNG_PROTOCOL_TASK_STACK_SIZE];
} task_stacks;

typedef struct netproto_inst_s {
    volatile netproto_status_t status;
    can5_netproto_t const *netproto;
    netif_inst_t *netif;                    // link to the netif instance to use
    task_t *task;                           // each protocol will have task
    QueueHandle_t tx_q;                     // tx queue for each message
    TAILQ_ENTRY(netproto_inst_s) te;
} netproto_inst_t;

typedef TAILQ_HEAD(netproto_inst_head_s, netproto_inst_s) netproto_inst_head_t;

/**
 * @brief Handler for __net module 
 * 
 */
static struct net_hdl_s {
    volatile can5_netmng_state_t status;
    struct {
        task_t task;
        EventGroupHandle_t evt_group;
    } netmng;
    netif_inst_head_t netif_list;
    netproto_inst_head_t netproto_list;
    task_t task[CAN5_NETPROTO_COUNT];
    struct {
        esp_event_handler_instance_t sensormng;
    } evt_hdl;
    can5_netproto_t const *protocol[CAN5_NETPROTO_COUNT];
} __net = {
    .status = NETMNG_STATE_UNINITD,
    .protocol = {
        &netproto_hazemon,
        &netproto_lwan,
        &netproto_mqtt,
    }
};

typedef struct active_proto_s {
    can5_netproto_type_t t_proto;       // protocol
    can5_netif_type_t t_netif;          // will use netif
} active_proto_t;


ESP_EVENT_DEFINE_BASE(CAN5_EVT_NET);

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
portTASK_FUNCTION(__task_netport, pv);

portTASK_FUNCTION(__task_netmng, pv);

static active_proto_t *__get_active_protocols(size_t *len);

static can5_err_t __allocate_netif();

static can5_err_t __allocate_protocols();

static can5_port_idx_t __get_netif_port(can5_netif_type_t netif_type);

static void __state_netproto_set(netproto_inst_t *netproto, netproto_status_t state);

static void __sensormng_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data);

static void __netif_conn_cb(const can5_net_connect_evt_t *evt);

static void __netproto_send_cb(const can5_netproto_evt_t *evt);

static void __packet_rx_cb(uint8_t netif_id, const void *data, size_t len);

//_______________________________________________________________________________________________________
//
//   FUNCTIONS DEFNITION
//-------------------------------------------------------------------------------------------------------

// TODO: clean this up for multiple protocols
can5_err_t init()
{
    netproto_inst_t *netproto;
    size_t count;

    if (__net.status == NETMNG_STATE_INITD) return CAN5_SUCCESS;


    TAILQ_INIT(&__net.netif_list);
    TAILQ_INIT(&__net.netproto_list);

    // add all netif to the nic list
    VERIFY_SUCCESS(__allocate_netif());

    VERIFY_SUCCESS(__allocate_protocols());

    // start a task for each net protocol
    count = 0;
    TAILQ_FOREACH(netproto, &__net.netproto_list, te) {
        netproto->task->hdl = xTaskCreateStatic(__task_netport,
                                                &can5_netproto_type_get_str(netproto->netproto->type)[5],
                                                CONFIG_CAN5_NETWORKMNG_PROTOCOL_TASK_STACK_SIZE,
                                                netproto,
                                                CONFIG_CAN5_NETWORKMNG_PROTOCOL_TASK_PRIORITY,
                                                task_stacks.proto[count++],
                                                &netproto->task->buffer);
        VERIFY_NOT_NULL(netproto->task->hdl);
        netproto->tx_q = xQueueCreate(1, sizeof(tx_q_msg_t));
        VERIFY_NOT_NULL(netproto->tx_q);
    }

    __net.netmng.task.hdl = xTaskCreateStatic(__task_netmng,
                                              "__task_netmng",
                                              CONFIG_CAN5_NETWORKMNG_TASK_STACK_SIZE,
                                              NULL,
                                              CONFIG_CAN5_NETWORKMNG_TASK_PRIORITY,
                                              task_stacks.netmng,
                                              &__net.netmng.task.buffer);

    __net.netmng.evt_group = xEventGroupCreate();

    VERIFY_NOT_NULL(__net.netmng.evt_group);




    esp_event_handler_instance_register(CAN5_EVT_SENSORMNG,
                                        ESP_EVENT_ANY_ID,
                                        __sensormng_evt_handler,
                                        NULL,
                                        &__net.evt_hdl.sensormng);
    __net.status = NETMNG_STATE_INITD;
    return CAN5_SUCCESS;
}

static can5_err_t pre_sleep()
{
    return CAN5_SUCCESS;
}

static can5_err_t post_sleep()
{
    return CAN5_SUCCESS;
}

static bool is_sleepable()
{
    return false;
}

static bool is_sleeping()
{
    return false;
}

static int32_t status_get()
{
    return (int32_t) __net.status;
}

__attribute__((unused)) static const char *_netif_details_getstr(const can5_netif_details_t *dt)
{
    static char ret[CAN5_NETIF_DET_STRLEN];
    if (!dt) {
        ESP_LOGE(TAG, "invalid pointer");
        return NULL;
    }
    snprintf(ret, CAN5_NETIF_DET_STRLEN, "Type: %d\n  Name: %s\n  Man:  %s\n  Ver:  %s\n  HWinfo:  %s\n", dt->type,
             dt->name, dt->manufacturer, dt->version, dt->hwinfo);
    return ret;
}


static can5_err_t run()
{
    return CAN5_SUCCESS;
}


can5_err_t uninit()
{
    if (__net.status == NETMNG_STATE_UNINITD) return CAN5_SUCCESS;
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    __net.status = NETMNG_STATE_UNINITD;
    return CAN5_SUCCESS;
}

static size_t is_connected(can5_netmng_connected_protos_t *connected_protos)
{
    netproto_inst_t *netproto_inst;
    size_t len = 0;
    TAILQ_FOREACH(netproto_inst, &__net.netproto_list, te) {
        connected_protos[len].proto_name = can5_netproto_type_get_str(netproto_inst->netproto->type);
        if (netproto_inst->status >= NETPROTO_STAT_CONNECTED) {
            connected_protos[len].is_connected = true;
            netproto_inst->netif->netif->ops.driverctl(CAN5_DRIVERCTL_NETIF_CONN_STATUS, NULL,
                                                       &connected_protos[len].status_str);
        }
        else {
            connected_protos[len].is_connected = false;
        }
        len++;
    }

    return len;
}

static bool is_active()
{
    return __net.status >= NETMNG_STATE_INITD;
}

/* ---------------------------------------------------------------------
 * Event Handlers
 -----------------------------------------------------------------------*/

static void __sensormng_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data)
{
    netproto_inst_t *netproto_inst;

    if (event_base != CAN5_EVT_SENSORMNG) return;

    switch (event_id) {
        case CAN5_SENSORMNG_EVT_CYCLE_SAVED:
            TAILQ_FOREACH(netproto_inst, &__net.netproto_list, te)  {
                if (netproto_inst->status == NETPROTO_STAT_RUNNING) {
                    netproto_inst->netproto->end();
                    __state_netproto_set(netproto_inst, NETPROTO_STAT_FAILED);
                }
            }
            xEventGroupSetBits(__net.netmng.evt_group, BIT(TASK_NOTIF_NEW_DATA));
            break;
    }

}

static void __netif_conn_cb(const can5_net_connect_evt_t *evt)
{
    netif_inst_t *netif_inst;
    netproto_inst_t *netproto_inst;

    VERIFY_NOT_NULL_VOID(evt);
    ESP_LOGI(TAG, "Connection event: %s on netif id %d", connevt_getstr(evt->type), evt->netif_id);
    NETIF_LIST_FIND_ID(evt->netif_id, netif_inst);

    if (!netif_inst) {
        return;
    }

    NETPROTO_FOREACH_NETIF(netproto_inst, netif_inst, {
        switch (evt->type) {

            case CAN5_NET_CONNEVT_NONE:
                break;

            case CAN5_NET_CONNEVT_CONNECTED:
                if (netproto_inst->status < NETPROTO_STAT_CONNECTED) {
                    vTaskResume(netproto_inst->task->hdl);
                    __state_netproto_set(netproto_inst, NETPROTO_STAT_CONNECTED);
                }
                break;

            case CAN5_NET_CONNEVT_SENDING:
                break;

            case CAN5_NET_CONNEVT_SEND_COMPLETE:
                break;

            case CAN5_NET_CONNEVT_SEND_FAILED:
                break;

            case CAN5_NET_CONNEVT_DISCONNECTED:
                __state_netproto_set(netproto_inst, NETPROTO_STAT_DISCONNECTED);
                break;

            case CAN5_NET_CONNEVT_DISCONNECT_TIMEOUT:
                break;

            case CAN5_NET_CONNEVT_CONNECT_TIMEOUT:
                break;

            case CAN5_NET_CONNEVT_CONNECT_FAILED:
                break;

            case CAN5_NET_CONNEVT_LAST:
                break;
        }

        netproto_inst->netproto->forward_netif_connevt(evt);
    })

    ESP_LOGI(TAG, "Connection event: %s", connevt_getstr(evt->type));

}

static void __netproto_send_cb(const can5_netproto_evt_t *evt)
{
    netproto_inst_t *netproto_inst;
    ESP_LOGI(TAG, "netproto event %d, id %d", evt->type, evt->netproto_id);

    NETPROTO_LIST_FIND_ID(evt->netproto_id, netproto_inst);

    if (!netproto_inst) {
        return;
    }


    switch (evt->type) {

        case CAN5_NETPROTO_EVT_NONE:

            break;

        case CAN5_NETPROTO_EVT_COMPLETE:

            if (netproto_inst->status == NETPROTO_STAT_RUNNING) {
                __state_netproto_set(netproto_inst, NETPROTO_STAT_COMPLETED);
            }
            break;

        case CAN5_NETPROTO_EVT_FAILED:

            if (netproto_inst->status == NETPROTO_STAT_RUNNING) {
                __state_netproto_set(netproto_inst, NETPROTO_STAT_FAILED);
            }
            break;

        case CAN5_NETPROTO_EVT_SEND_ATTEMPTS_OVER:
            break;

        case CAN5_NETPROTO_EVT_COUNT:
            break;
    }


}

static void __packet_rx_cb(uint8_t netif_id, const void *data, size_t len)
{
    ESP_LOGI(TAG, "received");
    netif_inst_t *netif_inst;
    netproto_inst_t *netproto_inst;

    NETIF_LIST_FIND_ID(netif_id, netif_inst);

    if (!netif_inst) {
        return;
    }

    NETPROTO_FOREACH_NETIF(netproto_inst, netif_inst,
            netproto_inst->netproto->forward_netif_rx(data, len));

}

/* ---------------------------------------------------------------------
 * Private functions
 -----------------------------------------------------------------------*/

static void __state_netproto_set(netproto_inst_t *netproto, netproto_status_t state)
{
    netproto->status = state;
}


static can5_port_idx_t __get_netif_port(can5_netif_type_t netif_type)
{
    switch (netif_type) {

        case CAN5_NET_TYPE_LWIP:
            return NETPORT_0;
        case CAN5_NET_TYPE_LORAWAN:
            return NETPORT_1;
        default:
            break;
    }
    return CAN5_PORT_NULL;
}

static netif_inst_t *__create_netif(can5_netif_type_t type)
{
    netif_inst_t *netif;
    netif = malloc(sizeof(netif_inst_t));

    if (!netif) {
        return NULL;
    }

    switch (type) {

        case CAN5_NET_TYPE_LWIP:
            netif->netif = &netif_wppr;
            break;
        case CAN5_NET_TYPE_LORAWAN:
            netif->netif = &netif_lwan;
            break;
        default:
            free(netif);
            return NULL;
    }

    netif->connected = false;

    netif->port = __get_netif_port(netif->netif->details.type);

    netif->netif->ops.register_recv_cb(__packet_rx_cb);

    return netif;
}

static can5_err_t __allocate_netif()
{
    bool wifi_enabled, lorawan_enabled, cellular_enabled;
    netif_inst_t *netif;
    size_t id;

    VERIFY_SUCCESS(config_manager.read_bool(CFG_WIFI_STA_ENABLE, &wifi_enabled));
    VERIFY_SUCCESS(config_manager.read_bool(CFG_LWAN_ENABLE, &lorawan_enabled));
    VERIFY_SUCCESS(config_manager.read_bool(CFG_CELL_ENABLE, &cellular_enabled));

    id = 0;

    if (wifi_enabled || cellular_enabled) {
        netif = __create_netif(CAN5_NET_TYPE_LWIP);
        VERIFY_NOT_NULL(netif);

        netif->netif->ops.set_id(id++);
        ESP_LOGI(TAG, "Allocating netif %s with id %d", can5_netif_getstr(CAN5_NET_TYPE_LWIP),
                 netif->netif->ops.get_id());
        TAILQ_INSERT_TAIL(&__net.netif_list, netif, te);
    }

    if (lorawan_enabled) {
        netif = __create_netif(CAN5_NET_TYPE_LORAWAN);
        VERIFY_NOT_NULL(netif);

        netif->netif->ops.set_id(id++);
        ESP_LOGI(TAG, "Allocating netif %s with id %d", can5_netif_getstr(CAN5_NET_TYPE_LORAWAN),
                 netif->netif->ops.get_id());
        TAILQ_INSERT_TAIL(&__net.netif_list, netif, te);
    }

    return CAN5_SUCCESS;
}

static can5_err_t __allocate_protocols()
{
    active_proto_t *active_protos;
    size_t active_protos_len;
    size_t task_idx;


    active_protos = __get_active_protocols(&active_protos_len);

    ESP_LOGI(TAG, "active protos len %d", active_protos_len);
    if (!active_protos_len) {
        // dont need to run any networking module
        return CAN5_SUCCESS;
    }

    task_idx = 0;

    for (size_t i = 0; i < active_protos_len; i++) {
        for (can5_netproto_type_t type = 0; type < CAN5_NETPROTO_COUNT; type++) {

            if (!__net.protocol[type]) break;

            if (active_protos[i].t_proto == __net.protocol[type]->type) {
                netif_inst_t *netif_cur;
                netproto_inst_t *netproto_inst;

                VERIFY_ALLOC(netproto_inst, sizeof(netproto_inst_t));

                netproto_inst->netproto = __net.protocol[type];

                TAILQ_FOREACH(netif_cur, &__net.netif_list, te) {
                    if (active_protos[i].t_netif == netif_cur->netif->details.type) {
                        netproto_inst->netif = netif_cur;
                        break;
                    }
                }
                ESP_LOGI(TAG, "Allocating protocol %s", can5_netproto_type_get_str(netproto_inst->netproto->type));
                ESP_LOGI(TAG, "%s %s", can5_netproto_type_get_str(__net.protocol[type]->type),
                         can5_netproto_type_get_str(active_protos[i].t_proto));

                // use statically allocated task
                netproto_inst->task = &__net.task[task_idx++];
                netproto_inst->status = NETPROTO_STAT_DISCONNECTED;

                netproto_inst->netproto->set_id(task_idx);

                TAILQ_INSERT_TAIL(&__net.netproto_list, netproto_inst, te);
            }
        }
    }

    return CAN5_SUCCESS;
}

// just a static variable to avoid heap usage
static active_proto_t __active_protocols[CAN5_NETPROTO_COUNT];

static active_proto_t *__get_active_protocols(size_t *len)
{
    bool hazemon_enabled, lorarelay_enabled, mqtt_enabled;
    *len = 0;

    config_manager.read_bool(CFG_HAZEMON_ENABLE, &hazemon_enabled);
    config_manager.read_bool(CFG_LORARELAY_ENABLE, &lorarelay_enabled);
    config_manager.read_bool(CFG_MQTT_DATA_ENABLE, &mqtt_enabled);

    if (hazemon_enabled) {
        __active_protocols[*len].t_proto = CAN5_NETPROTO_HAZEMON;
        __active_protocols[*len].t_netif = CAN5_NET_TYPE_LWIP;
        (*len)++;
    }

    if (lorarelay_enabled) {
        __active_protocols[*len].t_proto = CAN5_NETPROTO_LORARELAY;
        __active_protocols[*len].t_netif = CAN5_NET_TYPE_LORAWAN;
        (*len)++;
    }

    if (mqtt_enabled) {
        __active_protocols[*len].t_proto = CAN5_NETPROTO_MQTT;
        __active_protocols[*len].t_netif = CAN5_NET_TYPE_LWIP;
        (*len)++;
    }

    return __active_protocols;
}


portTASK_FUNCTION(__task_netport, pv)
{
    netproto_inst_t *netproto_inst = pv;
    netif_inst_t *netif_inst = netproto_inst->netif;
    can5_err_t ret;
    int retries;
    tx_q_msg_t msg;
    ESP_LOGI(TAG, "Starting task %s", can5_netproto_type_get_str(netproto_inst->netproto->type));

    if (!netif_inst) {
        ESP_LOGE(TAG, "No network interface enabled for %s!", can5_netproto_type_get_str(netproto_inst->netproto->type));
        vTaskDelete(NULL);
    }

    for (;;) {
        ret = CAN5_SUCCESS;

        switch (netproto_inst->status) {

            case NETPROTO_STAT_DISCONNECTED:
                if (netif_inst->connected) {
                    __state_netproto_set(netproto_inst, NETPROTO_STAT_CONNECTED);

                } else if (
                    netif_inst->netif->ops.detect(netif_inst->port) == CAN5_SUCCESS &&
                    netif_inst->netif->ops.init(netif_inst->port) == CAN5_SUCCESS &&
                    netif_inst->netif->ops.connect(__netif_conn_cb, false) == CAN5_SUCCESS
                    ) {
                    __state_netproto_set(netproto_inst, NETPROTO_STAT_CONNECTING);
                    netproto_inst->netproto->init(netif_inst->netif);
                }
                break;

            case NETPROTO_STAT_CONNECTING:
                vTaskSuspend(NULL);
                break;

            case NETPROTO_STAT_CONNECTED:

                netif_inst->connected = true;
                __state_netproto_set(netproto_inst, NETPROTO_STAT_READY);
                break;

            case NETPROTO_STAT_READY:
                xQueueReceive(netproto_inst->tx_q, &msg, portMAX_DELAY);

                __state_netproto_set(netproto_inst, NETPROTO_STAT_RUN);

                break;

            case NETPROTO_STAT_RUN:
                retries = 5;
                while (retries != 0) {
                    if ((ret =netproto_inst->netproto->send(msg.data, msg.len,
                                                      __netproto_send_cb)) != CAN5_SUCCESS) {
                        if (ret == CAN5_NET_ERR_PARSE_INCOMPLETE) {
                            // this mean we count it as success and discard the malformed data
                            __state_netproto_set(netproto_inst, NETPROTO_STAT_COMPLETED);
                            break;
                        } else {
                            CAN5_ERR_CHECK_NO_ABORT(ret);
                        }
                    }
                    else {
                        __state_netproto_set(netproto_inst, NETPROTO_STAT_RUNNING);
                        break;
                    }
                    retries--;
                }

                if (retries == 0) {
                    __state_netproto_set(netproto_inst, NETPROTO_STAT_DISCONNECTED);
                    xEventGroupSetBits(__net.netmng.evt_group, BIT(TASK_NOTIF_NETIF_FAILED));
                }

                break;

            case NETPROTO_STAT_RUNNING:
                break;

            case NETPROTO_STAT_COMPLETED:
                __state_netproto_set(netproto_inst, NETPROTO_STAT_READY);
                xEventGroupSetBits(__net.netmng.evt_group, BIT(TASK_NOTIF_NETIF_COMPLETED));
                break;

            case NETPROTO_STAT_FAILED:
                xEventGroupSetBits(__net.netmng.evt_group, BIT(TASK_NOTIF_NETIF_FAILED));
                __state_netproto_set(netproto_inst, NETPROTO_STAT_READY);
                break;
        }

        //netif_inst->netif->ops.run();
        netproto_inst->netproto->run();
        PRINT_TASK_HIGHWATER_MARK(NULL);

        //ESP_LOGI(TAG, "Netproto status %s", status_netproto_getstr(netproto_inst->status));
        taskYIELD();
        vTaskDelay(1);
    }
}

/*
 * This task should read data from defined source and send it through the network manager
 */

typedef enum send_data_res_e {
    SDR_COMPLETED = 0,
    SDR_FAILED,
    SDR_INVALID_DATA,
    SDR_NONE,
} send_data_res_t;

static send_data_res_t __send_data_netproto(const tx_q_msg_t *msg, netproto_inst_t *netproto_inst)
{
    EventBits_t bits;

    ESP_LOGI(TAG, "Initiating TX: %s", msg->data);

    xQueueSend(netproto_inst->tx_q, msg, portMAX_DELAY);

    bits = xEventGroupWaitBits(__net.netmng.evt_group,
                               BIT(TASK_NOTIF_NETIF_COMPLETED)    |
                               BIT(TASK_NOTIF_NETIF_FAILED)       |
                               BIT(TASK_NOTIF_NETIF_INVALID_DATA),
                               pdTRUE,
                               pdFALSE,
                               MAX_ACK_WAIT_TICK);

    if (bits & BIT(TASK_NOTIF_NETIF_COMPLETED)) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
        hal.set_status_led(true);
        vTaskDelay(pdMS_TO_TICKS(50));
        hal.set_status_led(false);
#endif
        ESP_LOGI(TAG, "Completed");
        return SDR_COMPLETED;
    }
    else if (bits & BIT(TASK_NOTIF_NETIF_FAILED)) {
        ESP_LOGI(TAG, "Failed");
        return SDR_FAILED;
    }
    else if (bits & BIT(TASK_NOTIF_NETIF_INVALID_DATA)) {
        ESP_LOGI(TAG, "Invalid data");
        return SDR_INVALID_DATA;
    }

    ESP_LOGI(TAG, "None");
    return SDR_NONE;
}

typedef struct netproto_success_s {
    can5_netproto_type_t type;
    bool success;
} netproto_success_t;

static bool __can_remove_from_storage(netproto_success_t *list, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (list[i].success) {
            return true;
        }
    }

    return false;
}

static buf_t netmng_sensor_data;

static void __coordinate_netproto_tasks(const uint8_t *sensor_data, size_t sensor_data_len)
{
    netproto_inst_t *netproto_inst;
    size_t success_list_count;
    send_data_res_t res;
    tx_q_msg_t msg;
    size_t netproto_count;
    bool remove_from_storage;


    msg.len = sensor_data_len;
    msg.id = esp_random();
    remove_from_storage = false;

    netproto_count = 0;

    TAILQ_FOREACH(netproto_inst, &__net.netproto_list, te) {
        netproto_count++;
    }

    netproto_success_t success_list[netproto_count];
    success_list_count = 0;

    TAILQ_FOREACH(netproto_inst, &__net.netproto_list, te) {

        if (netproto_inst->status >= NETPROTO_STAT_CONNECTED) {

            //msg.data = malloc(len);
            //memcpy(msg.data, data, len);
            msg.data = sensor_data;
            res = __send_data_netproto(&msg, netproto_inst);

            switch(res) {

                case SDR_COMPLETED:
                    success_list[success_list_count].type = netproto_inst->netproto->type;
                    success_list[success_list_count].success = true;
                    success_list_count++;
                    break;

                case SDR_FAILED:
                    success_list[success_list_count].type = netproto_inst->netproto->type;
                    success_list[success_list_count].success = false;
                    success_list_count++;
                    break;

                case SDR_INVALID_DATA:
                    remove_from_storage = true;
                    goto done;

                case SDR_NONE:
                default:
                    // do nothing
                    break;
            }
        }

    }

    // TODO: here we need to define the behaviour, if one succeeds or if one fails
    remove_from_storage = __can_remove_from_storage(success_list, success_list_count);

done:
    if (remove_from_storage) {

        CAN5_ERR_CHECK_NO_ABORT(can5_storage_search_and_pop_fs(SENSOR_DATA_TAG, sensor_data,
                                                                  sensor_data_len, STORAGE_STACK_MAX_SEARCH_DEPTH));
    }
}


portTASK_FUNCTION(__task_netmng, pv)
{
    ESP_LOGI(TAG, "Started Netmng task");
    can5_err_t ret;
    netproto_inst_t *netproto_inst;
    bool is_connected;

    for (;;) {
        // read from card
        is_connected = false;
        TAILQ_FOREACH(netproto_inst, &__net.netproto_list, te) {
            if (netproto_inst->status >= NETPROTO_STAT_CONNECTED) {
                is_connected = true;
            }
        }
        if (is_connected) {

            ret = can5_storage_peek_fs(SENSOR_DATA_TAG, netmng_sensor_data.buf, &netmng_sensor_data.len);

            if (ret == CAN5_SUCCESS) {
                netmng_sensor_data.buf[netmng_sensor_data.len] = '\0';
                __coordinate_netproto_tasks(netmng_sensor_data.buf, netmng_sensor_data.len);
                PRINT_TASK_HIGHWATER_MARK(NULL);

            } else if (ret == CAN5_STORAGE_ERR_EMPTY) {
                // wait for new data
                esp_event_post(CAN5_EVT_NET, CAN5_NET_EVT_SLEEP_UNTIL, NULL, 0, portMAX_DELAY);
                xEventGroupWaitBits(__net.netmng.evt_group,
                                    BIT(TASK_NOTIF_NEW_DATA),
                                    pdTRUE,
                                    pdTRUE,
                                    portMAX_DELAY);
            }
            else {
                //CAN5_ERR_CHECK_NO_ABORT(ret);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        PRINT_TASK_HIGHWATER_MARK(NULL);

        taskYIELD();
    }
}
//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0


/**
 * @brief Net Nmanager status tags
 * 
 */
static const can5_tag_tab_t __net_stat_tags = {
    TAG_TAB_ITEM(NETMNG_STATE_UNINITD),
    TAG_TAB_ITEM(NETMNG_STATE_INITD),
};

static const can5_tag_tab_t __netproto_stat_tags = {
    TAG_TAB_ITEM(NETPROTO_STAT_DISCONNECTED),
    TAG_TAB_ITEM(NETPROTO_STAT_CONNECTING),
    TAG_TAB_ITEM(NETPROTO_STAT_CONNECTED),
    TAG_TAB_ITEM(NETPROTO_STAT_READY),
    TAG_TAB_ITEM(NETPROTO_STAT_RUN),
    TAG_TAB_ITEM(NETPROTO_STAT_RUNNING),
    TAG_TAB_ITEM(NETPROTO_STAT_COMPLETED),
    TAG_TAB_ITEM(NETPROTO_STAT_FAILED),
};

static const can5_tag_tab_t __net_evt_tags = {
    TAG_TAB_ITEM(CAN5_NET_EVT_NONE),
    TAG_TAB_ITEM(CAN5_NET_EVT_INITIALIZED),
    TAG_TAB_ITEM(CAN5_NET_EVT_RUNNING),
    TAG_TAB_ITEM(CAN5_NET_EVT_BUSY),
    TAG_TAB_ITEM(CAN5_NET_EVT_COMPLETED),
    TAG_TAB_ITEM(CAN5_NET_EVT_RECVD),
    TAG_TAB_ITEM(CAN5_NET_EVT_LORAWAN_GOT_DEVEUI),
};

static const can5_tag_tab_t __net_connevt_tags = {
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_NONE),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_CONNECTED),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_SENDING),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_SEND_COMPLETE),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_SEND_FAILED),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_DISCONNECTED),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_DISCONNECT_TIMEOUT),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_CONNECT_TIMEOUT),
    TAG_TAB_ITEM(CAN5_NET_CONNEVT_CONNECT_FAILED),
};

static const char *status_getstr(int32_t status)
{
    return TAG_LOOKUP(status, __net_stat_tags);
}

__attribute__((unused)) static const char *status_netproto_getstr(netproto_status_t status)
{
    return TAG_LOOKUP(status, __netproto_stat_tags);
}

static const char *evt_getstr(int32_t evt)
{
    return TAG_LOOKUP(evt, __net_evt_tags);;
}

const char *connevt_getstr(can5_net_connect_evt_type_t type)
{
    return TAG_LOOKUP(type, __net_connevt_tags);
}

#endif
