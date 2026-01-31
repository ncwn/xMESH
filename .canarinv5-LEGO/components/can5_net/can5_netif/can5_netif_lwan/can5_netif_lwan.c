/**
 * @file   can5_netif_template.c
 * @author Luca De Mori (luca.demori.it@gmail.com)
 * @brief 
 * @version 0.1
 * @date    2021-02-10
 * 
 */

//_______________________________________________________________________________________________________
//
//   DEFINES
//-------------------------------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "can5_events.h"
#include "can5_netif_lwan.h"
#include "can5_hal.h"
#include "can5_utils.h"
#include "esp_event.h"
#include "esp_log.h"
#include "can5_config.h"
#include "can5_patch_lwan_ctx.h"


static const char *const TAG = "NETIF_LWAN";
#define LWAN_UART_SEND_TIMEOUT  5000                /* ms */
#define LWAN_CMD_DELAY          200                 /* ms */
#define LWAN_CMD_TX_Q_SIZE      2
#define LWAN_CMD_RX_Q_SIZE      8
#define LWAN_SAVE_CTX_INTERVAL  10                  /* save ctx after 10 sends */


#define DEBUG_RESET_NETIF       0

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

#define LWAN_TASK_STACK_SIZE                2540

#define CHECK_INITD() do {                       \
    if (__lwan.status == NETIF_LWAN_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; \
} while(0)

#define LWAN_NEWLINE            "\r\n"

#define CONNECTION_EVT(evt_type) {              \
    if (__lwan.conncb) {                        \
        can5_net_connect_evt_t evt = {          \
            .type = (evt_type),                 \
            .netif_id = get_id(),               \
        };                                      \
        __lwan.conncb(&evt);                    \
    }                                           \
}

#define ESP_LOGUART(TAG, fmt, args...)           ESP_LOGI(TAG, "\033[0;36m" fmt "\033[0m", args);


//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------

#define MAKE_DEF_TOKEN(token, flgs) { .str = token, .len = (sizeof(token) - 1), .flags = flgs }

#define LWAN_CMD_IS_ACTION          (1 << 0)
#define LWAN_CMD_IS_GET             (1 << 1)
#define LWAN_CMD_IS_SET             (1 << 2)
#define LWAN_CMD_IS_SEND            (1 << 3)

static const char *const lwan_aterror_description[] = {
    "OK",                     /* AT_OK */
    "AT_ERROR",               /* AT_ERROR */
    "AT_PARAM_ERROR",         /* AT_PARAM_ERROR */
    "AT_BUSY_ERROR",          /* AT_BUSY_ERROR */
    "AT_TEST_PARAM_OVERFLOW", /* AT_TEST_PARAM_OVERFLOW */
    "AT_NO_NETWORK_JOINED",   /* AT_NO_NET_JOINED */
    "AT_RX_ERROR",            /* AT_RX_ERROR */
    "AT_NO_CLASS_B_ENABLE",   /* AT_NO_CLASS_B_ENABLE */
    "AT_DUTYCYLE_RESTRICTED", /* AT_DUTYCYLE_RESTRICTED */
    "AT_CRYPTO_ERROR",        /* AT_CRYPTO_ERROR */
    "error unknown",          /* AT_MAX */
};

typedef enum lwan_aterror_e {
    LWAN_REP_AT_OK = 0,
    LWAN_REP_AT_ERROR,
    LWAN_REP_AT_PARAM_ERROR,
    LWAN_REP_AT_BUSY_ERROR,
    LWAN_REP_AT_TEST_PARAM_OVERFLOW,
    LWAN_REP_AT_NO_NET_JOINED,
    LWAN_REP_AT_RX_ERROR,
    LWAN_REP_AT_NO_CLASS_B_ENABLE,
    LWAN_REP_AT_DUTYCYCLE_RESTRICTED,
    LWAN_REP_AT_CRYPTO_ERROR,
    LWAN_REP_AT_UNKNOWN_ERROR,

    LWAN_REP_AT_COUNT,
    LWAN_REP_TIMEOUT_ERR,
    LWAN_REP_AT_NONE,
} lwan_aterror_t;

const static char *lwan_evts[] = {
    "+EVT:JOINED",                      // joined
    "+EVT:JOIN FAILED",                 // join failed
    "+EVT:RX_1,",                       // rssi, snr
    "+EVT:RX_2,",                       // rssi, snr
    "+EVT:21:",                        // data
    "+CTX=",                           // context
};

typedef enum lwan_evt_e {
    LWAN_EVT_JOINED = 0,
    LWAN_EVT_JOIN_FAILED,
    LWAN_EVT_RX1,
    LWAN_EVT_RX2,
    LWAN_EVT_RX,
    LWAN_EVT_CTX,

    LWAN_EVT_COUNT,
    LWAN_EVT_NONE,
} lwan_evt_t;

typedef enum netif_lwan_at_cmd_e {
    LWAN_AT_CMD_RESET,
    LWAN_AT_CMD_ECHO_OK,
    LWAN_AT_CMD_DEUI,
    LWAN_AT_CMD_DADDR,
    LWAN_AT_CMD_APPEUI,
    LWAN_AT_CMD_ADR,
    LWAN_AT_CMD_TXP,
    LWAN_AT_CMD_DR,
    LWAN_AT_CMD_RX2FQ,
    LWAN_AT_CMD_RX2DR,
    LWAN_AT_CMD_RX1DL,
    LWAN_AT_CMD_RX2DL,
    LWAN_AT_CMD_JN1DL,
    LWAN_AT_CMD_JN2DL,
    LWAN_AT_CMD_NJM,
    LWAN_AT_CMD_NWKID,
    LWAN_AT_CMD_CLASS,
    LWAN_AT_CMD_JOIN,
    LWAN_AT_CMD_NJS,
    LWAN_AT_CMD_SENDB,
    LWAN_AT_CMD_SEND,
    LWAN_AT_CMD_CFM,
    LWAN_AT_CMD_CFS,
    LWAN_AT_CMD_SNR,
    LWAN_AT_CMD_RSSI,
    LWAN_AT_CMD_CTX,
    LWAN_AT_CMD_VER,
    LWAN_AT_CMD_VL,

    LWAN_AT_CMD_CRLF,                                         /* send \r\n */
    LWAN_AT_CMD_MAX,

} netif_lwan_at_cmd_t;

/* This struct should be tightly coupled with netif_lwan_at_cmd_e */
static struct at_commands {
    const char *str;
    const size_t len;
    const uint8_t flags;
} at_cmds[] = {
    /* Reset the lora board */
    MAKE_DEF_TOKEN("ATZ", LWAN_CMD_IS_ACTION),
    /* Get OK status */
    MAKE_DEF_TOKEN("AT", LWAN_CMD_IS_ACTION | LWAN_CMD_IS_GET),
    /* Get the device EUI */
    MAKE_DEF_TOKEN("AT+DEUI", LWAN_CMD_IS_GET),
    /* Device Address */
    MAKE_DEF_TOKEN("AT+DADDR", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* App EUI */
    MAKE_DEF_TOKEN("AT+APPEUI", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Adaptive Data Rate , 0: Disabled, 1: Enabled */
    MAKE_DEF_TOKEN("AT+ADR", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Transmit Power , 0 - 5 */
    MAKE_DEF_TOKEN("AT+TXP", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Data Rate , 0 - 7 */
    MAKE_DEF_TOKEN("AT+DR", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* RX2 window frequency */
    MAKE_DEF_TOKEN("AT+RX2FQ", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Rx2 window data rate, 0 - 7 */
    MAKE_DEF_TOKEN("AT+RX2DR", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Delay between the end of tx and rx window 1 in ms */
    MAKE_DEF_TOKEN("AT+RX1DL", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Delay between the end of tx and rx window 2 in ms */
    MAKE_DEF_TOKEN("AT+RX2DL", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Join Accept Delay between the end of tx and join rx window 1 in ms */
    MAKE_DEF_TOKEN("AT+JN1DL", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Join Accept Delay between the end of tx and join rx window 2 in ms */
    MAKE_DEF_TOKEN("AT+JN2DL", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Network Join Mode, 0: ABP, 1: OTA */
    MAKE_DEF_TOKEN("AT+NJM", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Network id */
    MAKE_DEF_TOKEN("AT+NWKID", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Class of device */
    MAKE_DEF_TOKEN("AT+CLASS", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Join */
    MAKE_DEF_TOKEN("AT+JOIN", LWAN_CMD_IS_SET),
    /* Network Join Status, 0: Not Joined, 1: Joined */
    MAKE_DEF_TOKEN("AT+NJS", LWAN_CMD_IS_GET),
    /* Send Hex */
    MAKE_DEF_TOKEN("AT+SENDB", LWAN_CMD_IS_SEND | LWAN_CMD_IS_SET),
    /* Send Text */
    MAKE_DEF_TOKEN("AT+SEND", LWAN_CMD_IS_SEND | LWAN_CMD_IS_SET),
    /* Confirmation Mode, 0: No confirmation, 1: Confirmation */
    MAKE_DEF_TOKEN("AT+CFM", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Get confirmation status of last AT+SEND (0-1) */
    MAKE_DEF_TOKEN("AT+CFS", LWAN_CMD_IS_GET),
    /* SNR */
    MAKE_DEF_TOKEN("AT+SNR", LWAN_CMD_IS_GET),
    /* RSSI */
    MAKE_DEF_TOKEN("AT+RSSI", LWAN_CMD_IS_GET),
    /* Frame Counters [up]:[down] */
    MAKE_DEF_TOKEN("AT+CTX", LWAN_CMD_IS_GET | LWAN_CMD_IS_SET),
    /* Detect */
    MAKE_DEF_TOKEN("AT+VER", LWAN_CMD_IS_GET),
    /* Verbose level */
    MAKE_DEF_TOKEN("AT+VL", LWAN_CMD_IS_SET),

    /* CR+LF to emulate enter */
    MAKE_DEF_TOKEN(LWAN_NEWLINE, 0),
};
#define CMD_CRLF                        at_cmds[LWAN_AT_CMD_CRLF]

/**
 * @brief Netif status
 * 
 */
typedef enum netif_lwan_status_e {
    NETIF_LWAN_STAT_UNINITD = 0,                              /**< Uninitialized */
    NETIF_LWAN_STAT_PREJOIN_CONFIGURE,                        /**< Uninitialized */
    NETIF_LWAN_STAT_INITD,                                    /**< Initialized */
    NETIF_LWAN_STAT_CONNECTING,                               /**<  */
    NETIF_LWAN_STAT_SEND_READY,
    NETIF_LWAN_STAT_SENDING,
    NETIF_LWAN_STAT_SLEEP,                                    /**<  */

} netif_lwan_status_t;

typedef enum task_notif_e {
    NOTIF_GOT_CMD_STATUS = 0,
    NOTIF_INITILIZED,
    NOTIF_JOINED,
    NOTIF_JOIN_FAILED,
} task_notif_t;

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

// send bytes through the interface 
static can5_err_t send(const void *data, size_t len);

// initialize the module to be ready to connect
static can5_err_t uninit();

static can5_err_t recv(void *prxdata, size_t *len, uint16_t timeout);

// set the callback function to execute upon reception of a packet on a specific port
static can5_err_t register_recv_cb(can5_net_rxcb_f *);

// return true if connection is considered established
static bool is_connected(void);

// establish a connection with the server
static can5_err_t connect(can5_net_conncb_f *conncb, bool wait);

// return rssi if available in dbm
static can5_netif_rssi_t rssi_get(void);

// Get driver status
static int32_t status_get();


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0

/**
 * @brief Return a string corresponding to the status descruption
 * 
 * @return const char* status description 
 */
static const char *status_getstr(int32_t status);

#endif


static can5_err_t run();

// run extra commands
static can5_err_t driverctl(uint8_t request, void *params, void *response);

//_______________________________________________________________________________________________________
//
//   VARIABLES
//-------------------------------------------------------------------------------------------------------


/**
 * @brief Module available functions
 * 
 */
const can5_netif_t netif_lwan = {
    .ops = {
        .get_id = get_id,
        .set_id = set_id,
        .detect = detect,
        .send = send,
        .init = init,
        .uninit = uninit,
        .recv = recv,
        .register_recv_cb = register_recv_cb,
        .is_connected = is_connected,
        .connect = connect,
        .rssi_get = rssi_get,
        .status_get = status_get,
        .run = run,
        .driverctl = driverctl,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL > 0
        .status_getstr = status_getstr,
#endif
    },
    .details = {
        .type = CAN5_NET_TYPE_LORAWAN,
        .name = "Can5 NetIf Template",
        .version = "1.0",
        .manufacturer = "AIT intERLab",
    }
};

typedef struct lwan_uart_rx_buf_s {
    char buf[4096];         // 4k
    size_t len;
    lwan_aterror_t result;
} lwan_uart_rx_buf_t;

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
    StackType_t stack[LWAN_TASK_STACK_SIZE];
} task_t;

static struct netif_lwan2_hdl_s {
    volatile netif_lwan_status_t status;
    volatile bool pause;
    uint8_t id;
    can5_port_idx_t detected;
    can5_net_rxcb_f *rxcb;
    can5_net_conncb_f *conncb;
    lwan_uart_rx_buf_t uart_rx_buf;
    esp_event_handler_instance_t cmdr_evt;
    struct {
        task_t rx_task;
        task_t tx_task;
        EventGroupHandle_t evt_group;
        QueueHandle_t tx_q;
        QueueHandle_t rx_q;
        TaskHandle_t connect_task_hdl;
    } lwan_task;
    struct {
        char buf[4096];
        size_t len;
        size_t ctr; // number of sends
    } uart_tx_buf;
    struct {
        char buf[NETIF_LWAN_MAX_RECV_SIZE];
        size_t len;
    } net_rx_buf;

    struct {
        bool otaa;
        char app_eui[24];

        char daddr[12];
        char rx_1_delay[5];
        char rx_2_delay[5];

        char adr[2];        // 0 / 1
        char dr[2];         // 0 - 7
        char txp[2];         // 0 - 7
    } params;

    time_t last_rx;

} __lwan = {
    .status = NETIF_LWAN_STAT_UNINITD,
    .pause = false,
    .detected = CAN5_PORT_NULL,
    .rxcb = NULL,
    .conncb = NULL,
    .uart_rx_buf = {
        .buf = {0},
        .len = 0,
        .result = LWAN_REP_AT_NONE,
    },
    .uart_tx_buf = {
        .buf =  {0},
        .len = 0,
    },
    .net_rx_buf = {
        .buf = {0},
        .len = 0,
    },
    .params = {
        .app_eui = {0},
        .adr = "0",
        .dr = "5",
        .txp = "0",
    },
    .last_rx = 0,
};


/* ---------------------------------------------------------------------
 * Commands
 -----------------------------------------------------------------------*/
typedef enum cmd_msg_tx_type_e {
    CMD_INITIALIZE = 0,
    CMD_CONNECT,
    CMD_MSG_SEND,
    CMD_RESET_FRAME_COUNTERS,
} cmd_msg_tx_type_t;

typedef struct cmd_msg_tx_send_s {
    uint16_t port;
    bool confirmed;
    char *hex_data;
    size_t hex_data_len;
    time_t send_cycle_time; // tx, rx1, rx2
} cmd_msg_tx_send_t;

typedef struct cmd_msg_tx_s {
    cmd_msg_tx_type_t type;
    union {
        cmd_msg_tx_send_t send;
        // add other types here
    };
} cmd_msg_tx_t;

typedef struct cmd_msg_rx_s {
    char *buf;
    size_t len;
} cmd_msg_rx_t;

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
/* Tasks */
portTASK_FUNCTION(__task_lwan_rx, pv);

portTASK_FUNCTION(__task_lwan_tx, pv);

/* command related functions */
static void __state_set(netif_lwan_status_t next);
static void __initialize_netif();

/* TX related functions */
static void __add_cmd(cmd_msg_tx_t *cmd);

static can5_err_t __at_get_config(can5_port_idx_t port, netif_lwan_at_cmd_t cmd);

static can5_err_t __at_set_config(can5_port_idx_t port, netif_lwan_at_cmd_t cmd,
                                  const char *params);

__attribute__((unused)) static can5_err_t __at_action(can5_port_idx_t port, netif_lwan_at_cmd_t cmd);

__attribute__((unused)) static can5_err_t __at_write_raw(can5_port_idx_t port, const char *data, size_t len);

/* RX related function */
static void __read_rx(const can5_serial_pattern_event_data_t *pattern_data);

static void __clear_rx_buf();

static inline void __at_delay();

static EventBits_t __wait_for_reply(time_t timeout_ms, task_notif_t notif_bits);

static can5_err_t __at_drain_rx(can5_port_idx_t port);

static time_t __get_send_delay();

static void __cmdr_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data);


static void __store_fcnt(const char *ctx_str);
/* ---------------------------------------------------------------------
 * Function definitions
 -----------------------------------------------------------------------*/
static uint8_t get_id()
{
    return __lwan.id;
}

static void set_id(uint8_t id)
{
    __lwan.id = id;
}

/* rudementary detect */
static can5_err_t detect(can5_port_idx_t port)
{
    TRACE_FUNC;
    time_t start, now;
    char buf;
    size_t len;
    static char detect_buffer[256];
    static size_t detect_buffer_len;

    if (__lwan.status >= NETIF_LWAN_STAT_INITD && __lwan.detected == port) {
        return CAN5_SUCCESS;
    }

    VERIFY_SUCCESS(hal.enable(true, NETPORT_1));

    hal.reset_netport1();
    vTaskDelay(pdMS_TO_TICKS(5000));

    detect_buffer_len = 256;
    hal.serial_recv(&detect_buffer, &detect_buffer_len, port, 1000);
    ESP_LOGUART(TAG, "%s", detect_buffer);
    detect_buffer_len = 0;

    start = can5_time(NULL);
    VERIFY_SUCCESS(__at_get_config(port, LWAN_AT_CMD_VER));

    CLEAR_ARRAY(detect_buffer);
    detect_buffer_len = 0;
    // wait 30 seconds
    do {
        len = 1;
        hal.serial_recv(&buf, &len, port, 1000);

        if (len) {
            detect_buffer[detect_buffer_len++] = buf;
            if (strncmp(&detect_buffer[detect_buffer_len - 2], "OK", 2) == 0) {
                break;
            }
        }
        now = can5_time(NULL);
    } while (start + 2 > now);

    if (strstr(detect_buffer, "MW_LORAWAN_VERSION:")) {
        __lwan.detected = port;
        ESP_LOGI(TAG, "Lorawan Interface detected!");
        return CAN5_SUCCESS;
    }

    return CAN5_NET_ERR_INVALID_DRIVER;
}

static can5_err_t init(can5_port_idx_t port)
{
    TRACE_FUNC;
    bool otaa;
    char *dev_eui;
    char *app_eui;
    char *daddr;
    char *rx_1_delay;
    char *rx_2_delay;
    char *adr;
    char *dr;
    char *txp;
    int64_t gps_cycle;

    dev_eui = app_eui = daddr = rx_1_delay = rx_2_delay = adr = dr = txp = NULL;

    if (__lwan.detected != port) {
        VERIFY_SUCCESS(detect(port));   // verify that the module is present in the specifyied port
    }

    if (__lwan.status >= NETIF_LWAN_STAT_INITD) return CAN5_SUCCESS;  // do no re-init

    if (is_connected()) {
        return CAN5_SUCCESS;
    }

    VERIFY_SUCCESS(config_manager.read_bool(CFG_LWAN_OTAA, &otaa));

    VERIFY_SUCCESS(config_manager.read_int(CFG_LORARELAY_GPS_CYCLE, &gps_cycle));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_DEVEUI, (uint8_t **)&dev_eui, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_APPEUI, (uint8_t **)&app_eui, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_DADDR, (uint8_t **)&daddr, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_RX_1_DELAY, (uint8_t **)&rx_1_delay, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_RX_2_DELAY, (uint8_t **)&rx_2_delay, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_ADAPTIVE_DATA_RATE, (uint8_t **)&adr, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_DATA_RATE, (uint8_t **)&dr, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    VERIFY_SUCCESS_SAFERETURN(config_manager.read(CFG_LWAN_TRANSMIT_POWER, (uint8_t **)&txp, NULL),
                              FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp));

    __lwan.params.otaa = otaa;
    /* OTAA */
    strncpy(__lwan.params.app_eui, app_eui, 24);

    /* ABP */
    strncpy(__lwan.params.daddr, daddr, 12);
    strncpy(__lwan.params.rx_1_delay, rx_1_delay, 5);
    strncpy(__lwan.params.rx_2_delay, rx_2_delay, 5);

    strncpy(__lwan.params.adr, adr, 2);
    strncpy(__lwan.params.dr, dr, 2);
    strncpy(__lwan.params.txp, txp, 2);

    FREE_BULK(dev_eui, app_eui, daddr, rx_1_delay, rx_2_delay, adr, dr, txp);


#if DEBUG_RESET_NETIF
    config_manager.write(CFG_LWAN_FW_CTX0, (uint8_t *) "", 1);
    config_manager.write(CFG_LWAN_FW_CTX1, (uint8_t *) "", 1);
    config_manager.write(CFG_LWAN_FW_CTX2, (uint8_t *) "", 1);
    config_manager.write(CFG_LWAN_FW_CTX3, (uint8_t *) "", 1);
    config_manager.write(CFG_LWAN_FW_CTX4, (uint8_t *) "", 1);
    config_manager.write(CFG_LWAN_FW_CTX5, (uint8_t *) "", 1);
    config_manager.write(CFG_LWAN_FW_CTX6, (uint8_t *) "", 1);
#endif
    if (__lwan.status == NETIF_LWAN_STAT_UNINITD) {
        __lwan.lwan_task.evt_group = xEventGroupCreate();
        VERIFY_NOT_NULL(__lwan.lwan_task.evt_group);

        __lwan.lwan_task.tx_q = xQueueCreate(LWAN_CMD_TX_Q_SIZE, sizeof(cmd_msg_tx_t));
        VERIFY_NOT_NULL(__lwan.lwan_task.tx_q);
        __lwan.lwan_task.rx_q = xQueueCreate(LWAN_CMD_RX_Q_SIZE, sizeof(cmd_msg_rx_t));
        VERIFY_NOT_NULL(__lwan.lwan_task.rx_q);
        __lwan.lwan_task.rx_task.hdl = xTaskCreateStatic(__task_lwan_rx,
                                                         "lwan_rx_netif",
                                                         LWAN_TASK_STACK_SIZE,
                                                         NULL,
                                                         configMAX_PRIORITIES - 6,
                                                         __lwan.lwan_task.rx_task.stack,
                                                         &__lwan.lwan_task.rx_task.buffer);
        VERIFY_NOT_NULL(__lwan.lwan_task.rx_task.hdl);

        __lwan.lwan_task.tx_task.hdl = xTaskCreateStatic(__task_lwan_tx,
                                                         "lwan_tx_netif",
                                                         LWAN_TASK_STACK_SIZE,
                                                         NULL,
                                                         configMAX_PRIORITIES -8,
                                                         __lwan.lwan_task.tx_task.stack,
                                                         &__lwan.lwan_task.tx_task.buffer);
        VERIFY_NOT_NULL(__lwan.lwan_task.tx_task.hdl);

        VERIFY_SUCCESS(esp_event_handler_instance_register(CAN5_EVT_CMDR,
                                            ESP_EVENT_ANY_ID,
                                            __cmdr_evt_handler,
                                            NULL,
                                            &__lwan.cmdr_evt));

        CAN5_ERR_CHECK_NO_ABORT(hal.serial_char_detect_install('\n', __lwan.detected, __read_rx));
    }

    __lwan.status = NETIF_LWAN_STAT_PREJOIN_CONFIGURE;

    // reset
    hal.reset_netport1();
    __wait_for_reply(5000, BIT(NOTIF_GOT_CMD_STATUS));
    __at_delay();

    cmd_msg_tx_t  cmd_msg_tx = {
        .type = CMD_INITIALIZE,
    };

    __add_cmd(&cmd_msg_tx);

    // wait for initialization to be over
    __wait_for_reply(portMAX_DELAY, BIT(NOTIF_INITILIZED));
    __lwan.pause = false;

    __lwan.status = NETIF_LWAN_STAT_INITD;

    return CAN5_SUCCESS;
}

// initialize the module to be ready to connect
static can5_err_t uninit()
{
    TRACE_FUNC;

    if (__lwan.status == NETIF_LWAN_STAT_UNINITD) return CAN5_SUCCESS;

    __lwan.status = NETIF_LWAN_STAT_UNINITD;
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    return CAN5_SUCCESS;
}

static char send_data_hex[1024];

// send bytes through the interface
static can5_err_t send(const void *data, size_t len)
{
    TRACE_FUNC;

    if (__lwan.status < NETIF_LWAN_STAT_SEND_READY) {
        CONNECTION_EVT(CAN5_NET_CONNEVT_SEND_FAILED);
        return CAN5_ERR_INVALID_STATE;
    }

    if (__lwan.status > NETIF_LWAN_STAT_SEND_READY) {
        CONNECTION_EVT(CAN5_NET_CONNEVT_SEND_FAILED);
        return CAN5_NET_ERR_BUSY;
    }

    if (__lwan.pause) {
        CONNECTION_EVT(CAN5_NET_CONNEVT_SEND_FAILED);
        return CAN5_NET_ERR_BUSY;
    }

    __state_set(NETIF_LWAN_STAT_SENDING);

    CLEAR_ARRAY(send_data_hex);
    can5_bin_to_hex(data, send_data_hex, len);


    cmd_msg_tx_t msg;
    msg.type = CMD_MSG_SEND;
    msg.send.port = 21;
    // use internal data as send is non blocking
    msg.send.hex_data = &send_data_hex[0];
    msg.send.hex_data_len = len * 2;    // one byte is two hex characters

    msg.send.confirmed = false;
    msg.send.send_cycle_time = __get_send_delay();

    ESP_LOGI_V(TAG, "Sending %s with send delay %ld %p", (char *) send_data_hex, msg.send.send_cycle_time, msg.send.hex_data);

    __add_cmd(&msg);

    return CAN5_SUCCESS;
}


static can5_err_t recv(void *prxdata, size_t *len, uint16_t timeout)
{
    CHECK_INITD();
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    return CAN5_SUCCESS;
}

// set the callback function to execute upon reception of a packet on a specific port
static can5_err_t register_recv_cb(can5_net_rxcb_f *cb)
{
    VERIFY_NOT_NULL(cb);
    __lwan.rxcb = cb;
    return CAN5_SUCCESS;
}

// return true if connection is considered established
static bool is_connected(void)
{
    return __lwan.status >= NETIF_LWAN_STAT_SEND_READY;
}

// establish a connection with the server
static can5_err_t connect(can5_net_conncb_f *conncb, bool wait)
{
    TRACE_FUNC;

    CHECK_INITD();

    if (is_connected()) {
        return CAN5_SUCCESS;
    }


    if (__lwan.status != NETIF_LWAN_STAT_INITD) {
        return CAN5_ERR_INVALID_STATE;
    }

    __lwan.conncb = conncb;
    __lwan.status = NETIF_LWAN_STAT_CONNECTING;

    cmd_msg_tx_t cmd_msg_tx = {
        .type = CMD_CONNECT,
    };

    __add_cmd(&cmd_msg_tx);

    if (wait) {
        // TODO: blocking connect
    }


    return CAN5_SUCCESS;
}

// return rssi if available in dbm
can5_netif_rssi_t rssi_get(void)
{
    int64_t rssi = 0;
    if (__lwan.status == NETIF_LWAN_STAT_UNINITD) return CAN5_RSSI_UNAVAILABLE;
    config_manager.read_int(CFG_LWAN_STATS_RSSI, &rssi);
    return (can5_netif_rssi_t )rssi;
}

static can5_err_t run()
{
    return CAN5_SUCCESS;
}
static char status_str[64];
static can5_err_t driverctl(uint8_t request, void *params, void *response)
{
    TRACE_FUNC;

    cmd_msg_tx_t cmd;
    switch (request) {
        case CAN5_DRIVERCTL_NETIF_LWAN_GET_SEND_DELAY:
            *((time_t *) response) = __get_send_delay();
            break;

        case CAN5_DRIVERCTL_NETIF_LWAN_RESET_FRAME_COUNTERS:
            cmd.type = CMD_RESET_FRAME_COUNTERS;
            __add_cmd(&cmd);
            break;

        case CAN5_DRIVERCTL_NETIF_LWAN_CONN_STATUS: {
#if 0
            int64_t rssi, snr, fcnt_up, fcnt_down;
            size_t len;
#endif
            int64_t up;
            int64_t down;
            time_t last_rx;
            *(char **) response = status_str;
            CLEAR_ARRAY(status_str);
#if 0
            VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_STATS_SNR, &snr));
            VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_STATS_RSSI, &rssi));
#endif
            VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_STATS_LAST_ACK, (int64_t *) &last_rx));
            VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_STATS_FCNT_DOWN, &down));
            VERIFY_SUCCESS(config_manager.read_int(CFG_LWAN_STATS_FCNT_UP, &up));
#if 0
            len = snprintf(status_str, 64, "Rssi: %d, Snr: %d, FCnt_Up: %u, FCnt_Down: %u",
                     (int16_t)rssi, (int16_t)snr, (uint32_t )fcnt_up, (uint32_t )fcnt_down);

            last_rx = can5_time(NULL) - __lwan.last_rx;
            if (last_rx < 3600) {
                sprintf(&status_str[len], ", Last Rx: %ld sec", last_rx);
            }
#endif

            snprintf(status_str, 64, "FCnt_Up: %lld, FCnt_Down: %lld, Last_Rx: %ld",
                     up, down, last_rx);
        }
            break;

        default:
            return CAN5_ERR_INVALID_DRIVERCTL;
    }
    return CAN5_SUCCESS;
}

static int32_t status_get()
{
    return __lwan.status;
}

/* ---------------------------------------------------------------------
 * Private functions definitions
 -----------------------------------------------------------------------*/


static void __state_set(const netif_lwan_status_t next)
{
    __lwan.status = next;
}

static time_t __get_send_delay()
{
    char *val;
    time_t delay;
    val = NULL;
    if (config_manager.read(CFG_LWAN_SEND_DELAY, (uint8_t **) &val, NULL) != CAN5_SUCCESS) {
        delay = LWAN_SEND_DELAY_DEFAULT;
    } else {
        delay = strtol(val, NULL, 0);
        if (delay == 0) {
            delay = LWAN_SEND_DELAY_DEFAULT;
        }
    }

    if (val) {
        free(val);
    }

    delay = delay / 1000;
    delay += 4; // 2 seconds grace period

    return delay;
}

/* ---------------------------------------------------------------------
 * TX
 -----------------------------------------------------------------------*/

static void __add_cmd(cmd_msg_tx_t *cmd)
{
    TRACE_FUNC;
    xQueueSend(__lwan.lwan_task.tx_q, cmd, portMAX_DELAY);
}

static can5_err_t __at_drain_rx(can5_port_idx_t port)
{
    TRACE_FUNC;

    char byte;

    // flush serial
    size_t len = 1;
    do {
        hal.serial_recv(&byte, &len, port, 10);
        //ESP_LOGI(TAG, "len %c %u", byte, len);
    } while (len > 0);

    return CAN5_SUCCESS;
}

static can5_err_t __at_get_config(can5_port_idx_t port, netif_lwan_at_cmd_t cmd)
{
    TRACE_FUNC;

    struct at_commands *at_cmd = &at_cmds[cmd];

    assert(at_cmd->flags & LWAN_CMD_IS_GET);

    VERIFY_SUCCESS(__at_drain_rx(port));

    //__at_delay();

    __lwan.uart_tx_buf.len = snprintf(__lwan.uart_tx_buf.buf, 4096,
                                      "%s=?" LWAN_NEWLINE, at_cmd->str);


    VERIFY_SUCCESS(hal.serial_send(__lwan.uart_tx_buf.buf, __lwan.uart_tx_buf.len, port,
                                   LWAN_UART_SEND_TIMEOUT));

    remove_spaces(__lwan.uart_tx_buf.buf);
    ESP_LOGUART(TAG, "> %s", __lwan.uart_tx_buf.buf);

    __at_delay();

    return CAN5_SUCCESS;
}

static can5_err_t __at_set_config(can5_port_idx_t port, netif_lwan_at_cmd_t cmd, const char *params)
{
    TRACE_FUNC;

    struct at_commands *at_cmd = &at_cmds[cmd];

    assert(at_cmd->flags & LWAN_CMD_IS_SET);

    VERIFY_SUCCESS(__at_drain_rx(port));


    __lwan.uart_tx_buf.len = snprintf(__lwan.uart_tx_buf.buf, 4096,
                                      "%s=%s" LWAN_NEWLINE, at_cmd->str, params);

    VERIFY_SUCCESS(hal.serial_send(__lwan.uart_tx_buf.buf, __lwan.uart_tx_buf.len, port,
                                   LWAN_UART_SEND_TIMEOUT));

    remove_spaces(__lwan.uart_tx_buf.buf);
    ESP_LOGUART(TAG, "> %s", __lwan.uart_tx_buf.buf);
    __at_delay();

    return CAN5_SUCCESS;
}

__attribute__((unused)) static can5_err_t __at_action(can5_port_idx_t port, netif_lwan_at_cmd_t cmd)
{
    TRACE_FUNC;

    struct at_commands *at_cmd = &at_cmds[cmd];

    assert(at_cmd->flags & LWAN_CMD_IS_ACTION);

    VERIFY_SUCCESS(__at_drain_rx(port));

    //__at_delay();

    __lwan.uart_tx_buf.len = snprintf(__lwan.uart_tx_buf.buf, 4096,
                                      "%s" LWAN_NEWLINE, at_cmd->str);

    VERIFY_SUCCESS(hal.serial_send(__lwan.uart_tx_buf.buf, __lwan.uart_tx_buf.len, port,
                                   LWAN_UART_SEND_TIMEOUT));

    remove_spaces(__lwan.uart_tx_buf.buf);
    ESP_LOGUART(TAG, "> %s", __lwan.uart_tx_buf.buf);
    __at_delay();

    return CAN5_SUCCESS;

}

static can5_err_t __at_write_raw(can5_port_idx_t port, const char *data, size_t len)
{
    TRACE_FUNC;

    VERIFY_SUCCESS(__at_drain_rx(port));

    VERIFY_SUCCESS(hal.serial_send(data, len, port,
                                   LWAN_UART_SEND_TIMEOUT));

    char *str = strdup(data);
    remove_spaces(str);
    ESP_LOGUART(TAG, "> %s", str);
    free(str);

    __at_delay();

    return CAN5_SUCCESS;
}

static inline void __at_delay()
{
    TRACE_FUNC;
    vTaskDelay(pdMS_TO_TICKS(LWAN_CMD_DELAY));
}

#define RUN_AT(at_func, _timeout, _notif_bits, success_stmts){  \
    __clear_rx_buf();                                           \
    at_func;                                                    \
    EventBits_t _bits = __wait_for_reply(_timeout, _notif_bits);\
    if (((_bits & _notif_bits) == _notif_bits)  && (__lwan.uart_rx_buf.result == LWAN_REP_AT_OK))  {\
        curr_state++;                                           \
        success_stmts;                                          \
        retries = 3;                                            \
    }                                                           \
    else {                                                      \
        retries--;                                              \
    }                                                           \
}

#define RUN_CTX_AT(ctx_type)                               {    \
    char *_str_buf = NULL;                                      \
    config_manager.read((ctx_type), (uint8_t **)&_str_buf, NULL);  \
    if (_str_buf[1] != ':') {                                    \
        /* context data should have "d:.*", i.e. : as second character */  \
        /* TODO: handle wrong cases */                          \
        curr_state++;                                           \
        break;                                                  \
    }                                                           \
                                                                \
    if (_str_buf[0] == '2') {                                   \
        __store_fcnt(_str_buf);                                 \
    }                                                           \
                                                                \
    RUN_AT(                                                     \
    __at_set_config(__lwan.detected, LWAN_AT_CMD_CTX, _str_buf),\
    5000,                                                       \
    BIT(NOTIF_GOT_CMD_STATUS),                                  \
    );                                                          \
    free(_str_buf);                                             \
}

static void __initialize_netif()
{
    TRACE_FUNC;

    enum {
        SET_VL = 0,
        GET_DEVEUI,
        SET_CONTEXT_0,
        SET_CONTEXT_1,
        SET_CONTEXT_2,
        SET_CONTEXT_3,
        SET_CONTEXT_4,
        SET_CONTEXT_5,
        SET_CONTEXT_6,
        DONE
    } curr_state;

    int retries = 3;
    curr_state = SET_VL;

    while (curr_state != DONE && retries != 0) {
        switch (curr_state) {
            // set verbose level to 1
            case SET_VL:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_VL,
#if DEBUG_RESET_NETIF
                                       "2"
#else
                                       "0"
#endif
                                       ),
                       5000,
                       BIT(NOTIF_GOT_CMD_STATUS) , );

                break;

                // get device eui
            case GET_DEVEUI:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_DEUI),
                       5000,
                       BIT(NOTIF_GOT_CMD_STATUS),
                       {
                           remove_spaces(__lwan.uart_rx_buf.buf);
                           ESP_LOGI(TAG, "DEUI: %s", __lwan.uart_rx_buf.buf);
                           CAN5_ERR_CHECK_NO_ABORT(config_manager.write(CFG_LWAN_DEVEUI, (uint8_t *) __lwan.uart_rx_buf.buf,
                                                                        strlen(__lwan.uart_rx_buf.buf)));
                           esp_event_post(CAN5_EVT_NET, CAN5_NET_EVT_LORAWAN_GOT_DEVEUI , NULL, 0, portMAX_DELAY);

                       });
                break;

                // set lorawan context 0
            case SET_CONTEXT_0:
                RUN_CTX_AT(CFG_LWAN_FW_CTX0);
                break;

            case SET_CONTEXT_1:
                RUN_CTX_AT(CFG_LWAN_FW_CTX1);
                break;

            case SET_CONTEXT_2:
                RUN_CTX_AT(CFG_LWAN_FW_CTX2);
                break;

            case SET_CONTEXT_3:
                RUN_CTX_AT(CFG_LWAN_FW_CTX3);
                break;

            case SET_CONTEXT_4:
                RUN_CTX_AT(CFG_LWAN_FW_CTX4);
                break;

            case SET_CONTEXT_5:
                RUN_CTX_AT(CFG_LWAN_FW_CTX5);
                break;

            case SET_CONTEXT_6:
                RUN_CTX_AT(CFG_LWAN_FW_CTX6);
                break;

            case DONE:
                break;

            default:
                configASSERT(false);
                break;
        }
        taskYIELD();
    }

    if (curr_state == DONE) {
        xEventGroupSetBits(__lwan.lwan_task.evt_group, BIT(NOTIF_INITILIZED));
    }
    else {
        ESP_LOGE(TAG, "Cannot initialize!");
        configASSERT(false);
    }
}

static void __connect_netif_abp()
{
    TRACE_FUNC;

    EventBits_t bits;

    enum {
        GET_NJS = 0,
        SET_DEVADDR,
        SET_RX_1_DELAY,
        SET_RX_2_DELAY,
        JOIN,
        WAIT_FOR_JOIN,
        GET_CTX,
        SET_CONTEXT_0,
        SET_CONTEXT_1,
        SET_CONTEXT_2,
        SET_CONTEXT_3,
        SET_CONTEXT_4,
        SET_CONTEXT_5,
        SET_CONTEXT_6,
        SET_ADR,
        SET_DR,
        SET_TXP,
        GET_RX2_DELAY,
        DONE
    } curr_state;

    int retries = 3;

    curr_state = GET_NJS;

    while (curr_state != DONE && retries != 0) {
        switch (curr_state) {

            case GET_NJS:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_NJS),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                   {
                       ESP_LOGI(TAG, "NJS: %s", __lwan.uart_rx_buf.buf);
                       if (__lwan.uart_rx_buf.buf[0] == '1') {
                           curr_state = SET_ADR;
                           //curr_state++;
                       }
                   })
                break;

            case SET_DEVADDR:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_DADDR  , __lwan.params.daddr),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case SET_RX_1_DELAY:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_RX1DL , __lwan.params.rx_1_delay),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case SET_RX_2_DELAY:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_RX2DL , __lwan.params.rx_2_delay),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;
            case JOIN:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_JOIN, "0"),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case WAIT_FOR_JOIN:
                bits = __wait_for_reply(20000, BIT(NOTIF_JOINED) | BIT(NOTIF_JOIN_FAILED));

                if (bits & BIT(NOTIF_JOINED)) {
                    vTaskDelay(pdMS_TO_TICKS(2000)); // wait for 2 seconds
                    retries = 3;
                    curr_state =  GET_CTX;
                }
                else {
                    retries = 3; // keep on trying to join
                    curr_state = JOIN;
                }
                break;

            case GET_CTX:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_CTX),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),

                   curr_state = SET_ADR;
                );
                break;

                /* after patches */
            case SET_CONTEXT_0:
                RUN_CTX_AT(CFG_LWAN_FW_CTX0);
                break;

            case SET_CONTEXT_1:
                RUN_CTX_AT(CFG_LWAN_FW_CTX1);
                break;

            case SET_CONTEXT_2:
                RUN_CTX_AT(CFG_LWAN_FW_CTX2);
                break;

            case SET_CONTEXT_3:
                RUN_CTX_AT(CFG_LWAN_FW_CTX3);
                break;

            case SET_CONTEXT_4:
                RUN_CTX_AT(CFG_LWAN_FW_CTX4);
                break;

            case SET_CONTEXT_5:
                RUN_CTX_AT(CFG_LWAN_FW_CTX5);
                break;

            case SET_CONTEXT_6:
                RUN_CTX_AT(CFG_LWAN_FW_CTX6);
                break;

            case SET_ADR:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_ADR, __lwan.params.adr),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case SET_DR:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_DR, __lwan.params.dr),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case SET_TXP:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_TXP, __lwan.params.txp),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case GET_RX2_DELAY:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_RX2DL),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                   {
                       remove_spaces(__lwan.uart_rx_buf.buf);
                       ESP_LOGI(TAG, "RX2DL: %s", __lwan.uart_rx_buf.buf);
                       config_manager.write(CFG_LWAN_SEND_DELAY, (uint8_t *)__lwan.uart_rx_buf.buf, strlen(__lwan.uart_rx_buf.buf));
                   })
                break;


            case DONE:
                break;
            default:
                configASSERT(false);
                break;
        }
        taskYIELD();
    }

    if (curr_state == DONE) {
        CONNECTION_EVT(CAN5_NET_CONNEVT_CONNECTED);
        __lwan.status = NETIF_LWAN_STAT_SEND_READY;
    } else {
        CONNECTION_EVT(CAN5_NET_CONNEVT_CONNECT_FAILED);
        __lwan.status = NETIF_LWAN_STAT_INITD;
    }
}

static void __connect_netif_otaa()
{
    TRACE_FUNC;

    EventBits_t bits;

    enum {
        GET_NJS = 0,
        SET_APP_EUI,
        JOIN,
        WAIT_FOR_JOIN,
        GET_CTX,
        SET_CONTEXT_0,
        SET_CONTEXT_1,
        SET_CONTEXT_2,
        SET_CONTEXT_3,
        SET_CONTEXT_4,
        SET_CONTEXT_5,
        SET_CONTEXT_6,
        SET_ADR,
        SET_DR,
        SET_TXP,
        GET_RX2_DELAY,
        DONE
    } curr_state;
    int retries = 3;

    curr_state = GET_NJS;

    while (curr_state != DONE && retries != 0) {
        switch (curr_state) {

            case GET_NJS:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_NJS),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                   {
                       ESP_LOGI(TAG, "NJS: %s", __lwan.uart_rx_buf.buf);
                       if (__lwan.uart_rx_buf.buf[0] == '1') {
                           curr_state = SET_ADR;
                           //curr_state++;
                       }
                   })
                break;

            case SET_APP_EUI:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_APPEUI, __lwan.params.app_eui),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case JOIN:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_JOIN, "1"),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case WAIT_FOR_JOIN:

                bits = __wait_for_reply(20000, BIT(NOTIF_JOINED) | BIT(NOTIF_JOIN_FAILED));

                if (bits & BIT(NOTIF_JOINED)) {
                    vTaskDelay(pdMS_TO_TICKS(2000)); // wait for 2 seconds
                    retries = 3;
                    curr_state =  GET_CTX;
                }
                else {
                    retries = 3; // keep on trying to join
                    curr_state = JOIN;
                }
                break;

            case GET_CTX:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_CTX),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),

                   curr_state = SET_ADR;
                );
                break;

            /* after patches */
            case SET_CONTEXT_0:
                RUN_CTX_AT(CFG_LWAN_FW_CTX0);
                break;

            case SET_CONTEXT_1:
                RUN_CTX_AT(CFG_LWAN_FW_CTX1);
                break;

            case SET_CONTEXT_2:
                RUN_CTX_AT(CFG_LWAN_FW_CTX2);
                break;

            case SET_CONTEXT_3:
                RUN_CTX_AT(CFG_LWAN_FW_CTX3);
                break;

            case SET_CONTEXT_4:
                RUN_CTX_AT(CFG_LWAN_FW_CTX4);
                break;

            case SET_CONTEXT_5:
                RUN_CTX_AT(CFG_LWAN_FW_CTX5);
                break;

            case SET_CONTEXT_6:
                RUN_CTX_AT(CFG_LWAN_FW_CTX6);
                break;

            case SET_ADR:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_ADR, __lwan.params.adr),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case SET_DR:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_DR, __lwan.params.dr),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case SET_TXP:
                RUN_AT(__at_set_config(__lwan.detected, LWAN_AT_CMD_TXP, __lwan.params.txp),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                );
                break;

            case GET_RX2_DELAY:
                RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_RX2DL),
                   5000,
                   BIT(NOTIF_GOT_CMD_STATUS),
                   {
                       remove_spaces(__lwan.uart_rx_buf.buf);
                       ESP_LOGI(TAG, "RX2DL: %s", __lwan.uart_rx_buf.buf);
                       config_manager.write(CFG_LWAN_SEND_DELAY, (uint8_t *)__lwan.uart_rx_buf.buf, strlen(__lwan.uart_rx_buf.buf));
                   })
                break;


            case DONE:
                break;
            default:
                configASSERT(false);
                break;
        }
        taskYIELD();
    }

    if (curr_state == DONE) {
        CONNECTION_EVT(CAN5_NET_CONNEVT_CONNECTED);
        __lwan.status = NETIF_LWAN_STAT_SEND_READY;
    } else {
        CONNECTION_EVT(CAN5_NET_CONNEVT_CONNECT_FAILED);
        __lwan.status = NETIF_LWAN_STAT_INITD;
    }
}

static void __send(const cmd_msg_tx_t *cmd)
{
    TRACE_FUNC;

    char data_prefix[32];
    size_t len_prefix;
    int retries;
    time_t send_time, delay_time, now;

    enum {
        SEND = 0,
        WAIT_FOR_SEND,
        GET_CTX,
        DONE,
    } curr_state;

    len_prefix = snprintf(data_prefix, 32, "AT+SEND=%u:%u:", cmd->send.port, cmd->send.confirmed);

    if (!len_prefix) {
        CONNECTION_EVT(CAN5_NET_CONNEVT_SEND_FAILED);
        return;
    }

    memmove(cmd->send.hex_data + len_prefix, cmd->send.hex_data, cmd->send.hex_data_len);

    memcpy(cmd->send.hex_data, data_prefix, len_prefix);

    strcat(&cmd->send.hex_data[len_prefix], LWAN_NEWLINE);

    ESP_LOGI_V(TAG, "Sending %s with send delay %ld", (char *) cmd->send.hex_data, cmd->send.send_cycle_time);

    retries = 3;
    curr_state = SEND;

    send_time = 0;

    while (curr_state != DONE && retries != 0) {

        switch (curr_state) {

            case SEND:
                RUN_AT(__at_write_raw(__lwan.detected, cmd->send.hex_data,
                                      cmd->send.hex_data_len + len_prefix + strlen(LWAN_NEWLINE)),
                    5000,
                    BIT(NOTIF_GOT_CMD_STATUS),
                    {
                        CONNECTION_EVT(CAN5_NET_CONNEVT_SENDING);
                        send_time = can5_time_ms(NULL);
                        __lwan.uart_tx_buf.ctr += 1;
                        __lwan.uart_tx_buf.ctr %= LWAN_SAVE_CTX_INTERVAL;
                    }
                );
                vTaskDelay(pdMS_TO_TICKS(500)); // wit for .5 seconds anyhow
                break;

            case WAIT_FOR_SEND:
                delay_time =  __get_send_delay() * 1000;
                now = can5_time_ms(NULL);

                if ((send_time + delay_time) > now) {
                    vTaskDelay(pdMS_TO_TICKS((send_time + delay_time) - now));
                }
                curr_state++;
                break;

            case GET_CTX:
                if (__lwan.uart_tx_buf.ctr + 1 == LWAN_SAVE_CTX_INTERVAL) {
                    RUN_AT(__at_get_config(__lwan.detected, LWAN_AT_CMD_CTX),
                       5000,
                       BIT(NOTIF_GOT_CMD_STATUS),
                    );
                }
                else {
                    curr_state++;
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case DONE:
            default:
                break;
        }

        taskYIELD();
    }

    if (curr_state == DONE) {
        __state_set(NETIF_LWAN_STAT_SEND_READY);
        CONNECTION_EVT(CAN5_NET_CONNEVT_SEND_COMPLETE);
    } else {
        __state_set(NETIF_LWAN_STAT_SEND_READY);
        CONNECTION_EVT(CAN5_NET_CONNEVT_SEND_FAILED);
    }
}

static void __reset_frame_counters(const cmd_msg_tx_t *cmd)
{
    TRACE_FUNC;

    size_t str_len;
    char *str_buf;

    enum {
        SET_CONTEXT_2,
        DONE,
    } curr_state;

    str_buf = NULL;
    if (config_manager.read(CFG_LWAN_FW_CTX2, (uint8_t **)&str_buf, &str_len) != CAN5_SUCCESS) {
        ESP_LOGE(TAG, "Cannot read context FW to reset frame counters.");
        if (str_buf) {
            free(str_buf);
        }
        return;
    }

    if (str_buf[1] != ':') {
        ESP_LOGE(TAG, "Invalid FW2 to reset frame counters. %s", str_buf);
        return;
    }

    can5_patch_lwan_ctx_framecounters(str_buf, true, true);


    if (config_manager.write(CFG_LWAN_FW_CTX2, (uint8_t *) str_buf, str_len) != CAN5_SUCCESS) {
        ESP_LOGE(TAG, "Cannot write context FW to reset frame counters.");
        free(str_buf);
        return;
    }

    free(str_buf);

    int retries = 3;
    curr_state = SET_CONTEXT_2;

    while (curr_state != DONE && retries != 0) {
        switch(curr_state) {
            case SET_CONTEXT_2:
                RUN_CTX_AT(CFG_LWAN_FW_CTX2);
                break;

            case DONE:
                break;
        }

        taskYIELD();
    }

    if (curr_state == DONE) {
        ESP_LOGI(TAG, "Frame counters reset to 1.");
    }
    else {
        ESP_LOGE(TAG, "Cannot reset frame counters");
    }

}


portTASK_FUNCTION(__task_lwan_tx, pv)
{
    cmd_msg_tx_t cmd_msg;
    for (;;) {

        CLEAR_STRUCT(cmd_msg);

        if (xQueueReceive(__lwan.lwan_task.tx_q, &cmd_msg, portMAX_DELAY) == pdTRUE) {

            switch (cmd_msg.type) {

                case CMD_INITIALIZE:
                    __initialize_netif();
                    break;

                case CMD_CONNECT:
                    if (__lwan.params.otaa) {
                        __connect_netif_otaa();
                    }
                    else {
                        __connect_netif_abp();
                    }
                    break;

                case CMD_MSG_SEND:
                    __send(&cmd_msg);
                    break;

                case CMD_RESET_FRAME_COUNTERS:
                    __reset_frame_counters(&cmd_msg);
                    break;

            }
        }

        PRINT_TASK_HIGHWATER_MARK(NULL);
    }

}

/* ---------------------------------------------------------------------
 * RX
 -----------------------------------------------------------------------*/


static void __clear_rx_buf()
{
    CLEAR_ARRAY(__lwan.uart_rx_buf.buf);
    __lwan.uart_rx_buf.len = 0;
    __lwan.uart_rx_buf.result = LWAN_REP_AT_NONE;
    xEventGroupClearBits(__lwan.lwan_task.evt_group, BIT(NOTIF_GOT_CMD_STATUS));
}

static void __add_cmd_rx(cmd_msg_rx_t *cmd)
{
    xQueueSend(__lwan.lwan_task.rx_q, cmd, portMAX_DELAY);
}


static char rx_buffer[LWAN_CMD_RX_Q_SIZE][CAN5_STORAGE_MAX_LEN];
static size_t rx_buffer_idx;

static void __read_rx(const can5_serial_pattern_event_data_t *pattern_data)
{
    TRACE_FUNC;

    if (pattern_data->port != __lwan.detected) {
        /* not ours */
        return;
    }

    char *sentence;
    can5_err_t ret;
    size_t _len;
    int retries;

    _len = pattern_data->pattern_pos + 1;

    /*
     * TODO: as far as I know. Event handlers are called sequentally from an event loop task
     * so we dont need mutex here.
     */
    sentence = rx_buffer[rx_buffer_idx];
    rx_buffer_idx = (rx_buffer_idx + 1) % LWAN_CMD_RX_Q_SIZE;

    retries = 3;

    //memset(sentence, 0, CAN5_STORAGE_MAX_LEN);
    while (--retries && (ret = hal.serial_recv(sentence, &_len, pattern_data->port, 0)) != CAN5_SUCCESS) {
        CAN5_ERR_CHECK_NO_ABORT(ret);
    }
    if (_len) {
        sentence[_len] = '\0';
        cmd_msg_rx_t cmd_msg = {
            .buf = sentence,
            .len = _len,
        };
        __add_cmd_rx(&cmd_msg);
    }
}


static EventBits_t __wait_for_reply(time_t timeout_ms, task_notif_t notif_bits)
{

    EventBits_t  bits = xEventGroupWaitBits(__lwan.lwan_task.evt_group,
                               notif_bits,
                               pdTRUE,
                               pdFALSE,
                               pdMS_TO_TICKS(timeout_ms));

    return bits;
}

static can5_err_t __process_rx_at_cmd(const char *str, size_t len)
{
    for (lwan_aterror_t t = 0; t < LWAN_REP_AT_COUNT; t++) {
        size_t errdesc_len = strlen(lwan_aterror_description[t]);
        if (len < errdesc_len) {
            continue;
        }

        if (strncmp(str, lwan_aterror_description[t], errdesc_len) == 0) {
            __lwan.uart_rx_buf.result = t;
            return CAN5_SUCCESS;
        }
    }

    return CAN5_NET_ERR_PARSE_INCOMPLETE;
}


static void __process_rssi(const char *evt_str, size_t len)
{
    char *s_dr, *s_rssi, *s_snr;
    int dr = 0, rssi = 0, snr = 0;

    s_dr = s_rssi = s_snr = NULL;

    s_dr = strnstr(evt_str, "DR", len);
    s_rssi = strnstr(evt_str, "RSSI", len);
    s_snr = strnstr(evt_str, "SNR", len);

    if (s_dr) {
        s_dr += 2;
        dr = strtol(s_dr, NULL, 10);
    }

    if (s_rssi) {
        s_rssi += 4;
        rssi = strtol(s_rssi, NULL, 10);
    }

    if (s_snr) {
        s_snr += 3;
        snr = strtol(s_snr, NULL, 10);
    }
    ESP_LOGI(TAG, "DR %d RSSI %d SNR %d", dr, rssi, snr);

    config_manager.write_int(CFG_LWAN_STATS_SNR, snr);
    config_manager.write_int(CFG_LWAN_STATS_RSSI, rssi);
}

static void __process_rx_data(const char *evt_str, size_t len)
{
    size_t bin_len = 0;
    char *hex, *end, *bin;
    hex = end = bin = NULL;

    bin_len = strtol(evt_str, &end, 16);

    if (!end || evt_str == end) {
        // illegal
        return;
    }

    hex = end + 1;

    if ((hex - evt_str) + strlen(LWAN_NEWLINE) + (bin_len * 2) != len) {
        // illegal
        return;
    }

    CLEAR_ARRAY(__lwan.net_rx_buf.buf);
    __lwan.net_rx_buf.len = 0;

    can5_hex_to_bin(hex, __lwan.net_rx_buf.buf, bin_len);
    __lwan.net_rx_buf.len = bin_len;
    __lwan.last_rx = can5_time(NULL);

    // write the last ack
    config_manager.write_int(CFG_LWAN_STATS_LAST_ACK, time(NULL));

    if (__lwan.rxcb) {
        __lwan.rxcb(__lwan.id, __lwan.net_rx_buf.buf, __lwan.net_rx_buf.len);
    }
}

static void __store_fcnt(const char *ctx_str)
{
    uint32_t fcnt_up, fcnt_down;
    can5_patch_lwan_ctx_get_framecounters(ctx_str, &fcnt_up, &fcnt_down);
    ESP_LOGI(TAG, "FCNT UP %u DOWN %u", fcnt_up, fcnt_down);
    ESP_LOGI(TAG, "%s", can5_err_to_str(config_manager.write_int(CFG_LWAN_STATS_FCNT_UP, (int64_t)fcnt_up)));
    ESP_LOGI(TAG, "%s", can5_err_to_str(config_manager.write_int(CFG_LWAN_STATS_FCNT_DOWN, (int64_t)fcnt_down)));
}

static void __process_rx_fw_ctx(const char *evt_str, size_t len)
{
    /* CTX will be +CTX=1:2:aabb
     * +CTX=[ctx_id]:data_len:data
     */

    // asci to int
    size_t index = evt_str[0] - '0';

    // check ':' in 2nd position
    if (evt_str[1] != ':') {
        ESP_LOGE(TAG, "Invalid CTX=%s", evt_str);
        return;
    }

    char *str = strdup(evt_str);

    //if (index == 0) {
    //    can5_patch_lwan_ctx_dwell_time(str, false, false);
    //}
    if (index == 2) {
        __store_fcnt(str);
    }

    // remove '/r/n'
    if (config_manager.write(CFG_LWAN_FW_CTX0 + index, (uint8_t *) str, len - 2) != CAN5_SUCCESS) {
        ESP_LOGE(TAG, "Cannot write context FW - %s.", evt_str);
        free(str);
    }

    free(str);

}

static void __rx_evt(const char *evt_str, size_t len)
{
    ESP_LOGI_V(TAG, "Evt %s len %d", evt_str, len);
    char *d_str;

    lwan_evt_t lwan_evt = LWAN_EVT_NONE;

    for (lwan_evt_t evt = 0; evt < LWAN_EVT_COUNT; evt++) {
        if (strncmp(evt_str, lwan_evts[evt], strlen(lwan_evts[evt])) == 0) {
            lwan_evt = evt;
        }
    }

    if (lwan_evt == LWAN_EVT_NONE) {
        return;
    }

    d_str = (char *) evt_str;
    switch (lwan_evt) {

        case LWAN_EVT_JOINED:
            ESP_LOGI(TAG, "Evt joined %s", d_str);
            xEventGroupSetBits(__lwan.lwan_task.evt_group, BIT(NOTIF_JOINED));
            break;

        case LWAN_EVT_JOIN_FAILED:
            ESP_LOGI(TAG, "Evt join failed %s", d_str);
            xEventGroupSetBits(__lwan.lwan_task.evt_group, BIT(NOTIF_JOIN_FAILED));
            break;

        case LWAN_EVT_RX1:
            d_str += strlen(lwan_evts[LWAN_EVT_RX1]);
            ESP_LOGI_V(TAG, "LWAN_EVT_RX1: %s", d_str);
            __process_rssi(d_str, len - (d_str - evt_str));
            break;

        case LWAN_EVT_RX2:
            d_str += strlen(lwan_evts[LWAN_EVT_RX2]);
            ESP_LOGI_V(TAG, "LWAN_EVT_RX2: %s", d_str);
            __process_rssi(d_str, len - (d_str - evt_str));
            break;

        case LWAN_EVT_RX:
            d_str += strlen(lwan_evts[LWAN_EVT_RX]);
            ESP_LOGI_V(TAG, "LWAN_EVT_RX: %s", d_str);
            __process_rx_data(d_str, len - (d_str - evt_str));
            break;

        case LWAN_EVT_CTX:
            d_str += strlen(lwan_evts[LWAN_EVT_CTX]);
            ESP_LOGI_V(TAG, "LWAN_EVT_CTX: %s", d_str);
            __process_rx_fw_ctx(d_str, len - (d_str - evt_str));
            break;
        default:
            break;
    }
}

static void __rx(const char *buf, size_t len)
{
    TRACE_FUNC;

    if (strcmp(buf, LWAN_NEWLINE) != 0) {
        ESP_LOGUART(TAG, "< %.*s", strlen(buf)-1, buf);
    }

    if (buf[0] == '+') {
        __rx_evt(buf, len);
    } else {
        // rest
        if (__process_rx_at_cmd(buf, len) == CAN5_SUCCESS) {
            // got result for the previous command
            xEventGroupSetBits(__lwan.lwan_task.evt_group, BIT(NOTIF_GOT_CMD_STATUS));
        } else {
            // append to the output
            strcat(__lwan.uart_rx_buf.buf, buf);
        }
    }
}


portTASK_FUNCTION(__task_lwan_rx, pv)
{
    cmd_msg_rx_t cmd_msg;
    for (;;) {
        if (xQueueReceive(__lwan.lwan_task.rx_q, &cmd_msg, portMAX_DELAY) == pdTRUE) {
            __rx(cmd_msg.buf, cmd_msg.len);
        }
        PRINT_TASK_HIGHWATER_MARK(NULL);
    }
}

/* ---------------------------------------------------------------------
 * Event Handlers
 -----------------------------------------------------------------------*/

static void __cmdr_evt_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                    void *event_data)
{
    if (event_base != CAN5_EVT_CMDR) return;
    switch (event_id) {
        case CAN5_CMDR_EVT_LWAN_PAUSE:
            __lwan.pause = true;
            break;

        case CAN5_CMDR_EVT_LWAN_RESUME:
            __lwan.pause = false;
            break;

        case CAN5_CMDR_EVT_LWAN_RESET_FRAME_COUNT:
            driverctl(CAN5_DRIVERCTL_NETIF_LWAN_RESET_FRAME_COUNTERS, NULL, NULL);
            break;
    }

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
static const can5_tag_tab_t _NETIF_LWAN2_STAT_tags = {
    TAG_TAB_ITEM(NETIF_LWAN_STAT_UNINITD),
    TAG_TAB_ITEM(NETIF_LWAN_STAT_PREJOIN_CONFIGURE),
    TAG_TAB_ITEM(NETIF_LWAN_STAT_INITD),
    TAG_TAB_ITEM(NETIF_LWAN_STAT_CONNECTING),
    TAG_TAB_ITEM(NETIF_LWAN_STAT_SEND_READY),
    TAG_TAB_ITEM(NETIF_LWAN_STAT_SENDING),
    TAG_TAB_ITEM(NETIF_LWAN_STAT_SLEEP),
};


static const char *status_getstr(int32_t status)
{
    return TAG_LOOKUP(status, _NETIF_LWAN2_STAT_tags);
}


#endif

