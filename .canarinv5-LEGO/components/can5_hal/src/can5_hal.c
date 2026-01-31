/*******************************************************************************
 * Author: Luca De Mori @liuked
 * Date:   27-03-2020
 * 
 * File:  can5_hal.s
 * Descr: Serving function 
 *******************************************************************************/


#include <stdint.h>
#include <string.h>
#include <can5_events.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_event.h"
#include "can5_hal.h"
#include "can5_utils.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_adc_cal.h"
#include "can5_rtc.h"
#include "esp_modem_config.h"
#include "esp_modem_c_api_types.h"
#include "esp_modem_api.h"
#include "../../can5_cmdr/include/can5_cmdr.h"

#if CONFIG_CAN5_HAL_SNTP_TIME_SYNC
#pragma message("Time Sync with SNTP enabled.")
#include <lwip/apps/sntp.h>
#include <esp_sntp.h>
#include <driver/spi_common.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <nvs_flash.h>
#include <esp_netif_ppp.h>

#endif

//_______________________________________________________________________________________________________
//
//   DEFINES
//-------------------------------------------------------------------------------------------------------

#define TAG "HAL"  

// port utils
#define IS_NETPORT(port)        (port>=NETPORT_1 && port<NETPORT_COUNT) // NETPORT_0 is dedicated to Wifi
#define IS_UPORT(port)          (port>=UPORT_0   && port<UPORT_COUNT)
#if CONFIG_IDF_TARGET_ESP32
#define IS_ADPORT(port)         (port>=ADPORT_0  && port<ADPORT_COUNT)
#define PORT_VALID(port)        (IS_UPORT(port)  || IS_ADPORT(port) || IS_NETPORT(port))
#elif CONFIG_IDF_TARGET_ESP32S3
#define PORT_VALID(port)        (IS_UPORT(port) || IS_NETPORT(port))
#endif
#define PORT_ENABLED(port)      (__hhal.enabled[port]])

#define LATCH_OUT_DELAY      pdMS_TO_TICKS(10)
#define MUX_INHIB_DELAY      pdMS_TO_TICKS(10)
#define MUX_SELECT_DELAY     pdMS_TO_TICKS(10)

#define UART_EVENTS_QUEUE_SIZE  10


/** @brief Get port type from port index */
#if CONFIG_IDF_TARGET_ESP32
#define PORT_TYP_GET(port)      (IS_NETPORT(port) ? CAN5_PORT_NET : (IS_ADPORT(port)? CAN5_PORT_AD : (IS_UPORT(port) ? CAN5_PORT_U : CAN5_PORT_OOB ) ) )
#elif CONFIG_IDF_TARGET_ESP32S3
#define PORT_TYP_GET(port)      (IS_NETPORT(port) ? CAN5_PORT_NET : (IS_UPORT(port) ? CAN5_PORT_U : CAN5_PORT_OOB ) )
#endif

/** @brief Get Pointer to  port handler for specific port type */
#if CONFIG_IDF_TARGET_ESP32
#define PORT_TYP_HDL_GETP(typ)  (typ==CAN5_PORT_NET ? &__hhal.netport : (typ==CAN5_PORT_U ? &__hhal.uport : (typ==CAN5_PORT_AD ? &__hhal.adport : NULL) ) )
#elif CONFIG_IDF_TARGET_ESP32S3
#define PORT_TYP_HDL_GETP(typ)  (typ==CAN5_PORT_NET ? &__hhal.netport : (typ==CAN5_PORT_U ? &__hhal.uport :  NULL) )
#endif

/** @brief Get Pointer to  port config for specific port type */
#define PORT_TYP_SERIAL_CONFIG_GETP(typ)  (typ==CAN5_PORT_NET ? &__hhal.netport.config.net.serial : (typ==CAN5_PORT_U ? &__hhal.uport.config.u.serial : NULL) ) 

/** @brief Derive Pointer to  port handler from port index */
#define PORT_HDL_GETP(port)      PORT_TYP_HDL_GETP(PORT_TYP_GET(port)) 

/** @brief Get Pointer to serial port config for specific port type */
#define PORT_SERIAL_CONFIG_GETP(port)       PORT_TYP_SERIAL_CONFIG_GETP(PORT_TYP_GET(port))



#define __is_pattern_detect_active(pctl) (pctrl->pattern_installed != CAN5_PORT_NULL)


/** latch output new */
#define LATCH_OUT(le_pin);\
gpio_set_level(le_pin, 1);\
vTaskDelay(LATCH_OUT_DELAY);\
gpio_set_level(le_pin, 0);

#if CONFIG_IDF_TARGET_ESP32
#define CHECK_ADPORT(port)      do { if (!IS_ADPORT(port)) return CAN5_HAL_ERR_INVALID_PORT; } while(0)
#endif
#define CHECK_UPORT(port)       do { if (!IS_UPORT(port)) return CAN5_HAL_ERR_INVALID_PORT; } while(0)
#define CHECK_SERIALPORT(port)  do { if (!IS_UPORT(port) && !IS_NETPORT(port)) return CAN5_HAL_ERR_INVALID_PORT; } while(0)
#define CHECK_VALID_PORT(port)  do { if (!PORT_VALID(port)) return CAN5_HAL_ERR_INVALID_PORT; } while(0)

#define CHECK_INITD()          do { if (__hhal.status == HAL_STAT_UNINITD) return CAN5_ERR_INVALID_STATE; } while(0)

#define CAN5_NETPORT_TX_BUFSZ       1024   //CONFIG_CAN5_NETPORT_TX_BUFSZ
#define CAN5_NETPORT_RX_BUFSZ       1024   //CONFIG_CAN5_NETPORT_RX_BUFSZ
#define CAN5_NETPORT_UART_BAUDRATE  9600  //CONFIG_CAN5_NETPORT_UART_BAUDRATE

#define CAN5_UPORT_TX_BUFSZ       1024 //CONFIG_CAN5_UPORT_TX_BUFSZ
#define CAN5_UPORT_RX_BUFSZ       1024 //CONFIG_CAN5_UPORT_RX_BUFSZ
#define CAN5_UPORT_UART_BAUDRATE  9600  //CONFIG_CAN5_NETPORT_UART_BAUDRATE

#if CONFIG_IDF_TARGET_ESP32
#define SPI_DMA_CHAN    1
#else
#define SPI_DMA_CHAN    SDSPI_DEFAULT_DMA
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define HAL_TASK_STACK_SIZE       4096
#else
#define HAL_TASK_STACK_SIZE       2048
#endif

//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------

/**
 * @brief Hal status
 * 
 */
typedef enum can5_hal_status_e {
    HAL_STAT_UNINITD = 0,  /**< Uninitialized */
    HAL_STAT_INITD,        /**< Initialized */

    HAL_STAT_LAST,
} can5_hal_status_t;

/**
 * @brief Wheter the AD port is currently difìgital or analog
 * 
 */
typedef enum can5_port_type_e {
    CAN5_PORT_OOB = 0,
    CAN5_PORT_NET,
#if CONFIG_IDF_TARGET_ESP32
    CAN5_PORT_AD,
#endif
    CAN5_PORT_U
} can5_port_type_t;

#if CONFIG_IDF_TARGET_ESP32
/**
 * @brief Wheter the AD port is currently difìgital or analog
 * 
 */
typedef enum can5_adport_type_e {
    ADPORT_TYP_NONE = 0,
    ADPORT_TYP_ANALOG,
    ADPORT_TYP_DIGITAL,
} can5_adport_type_t;
#endif

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
    StackType_t stack[HAL_TASK_STACK_SIZE];
} task_t;

typedef struct can5_serial_port_ctrl_s {
    QueueHandle_t    uart_queue;
    const int        uart;
    const can5_pin_t tx;
    const can5_pin_t rx;
    size_t           tx_bufsz;
    size_t           rx_bufsz;
    uart_config_t    uart_config;
    volatile can5_port_idx_t  pattern_installed;
    can5_serial_pattern_func  pattern_cb;
} can5_serial_port_ctrl_t;

typedef struct can5_port_hdl_s {

    can5_port_type_t type;

    union can5_port_config_u {
#if CONFIG_IDF_TARGET_ESP32
        /** AD port support */
        struct can5_adport_config_s {
            can5_adport_type_t ad_type;
            const can5_muxd_port_wiring_t mux;  /** Muxed port support */
            //const can5_pin_t le;                /** port latch enable (used to latch the EN signal before changing port) */
            const can5_pin_t en;
        } ad;
#endif

        /** U port support */
        struct can5_uport_config_s {
            can5_serial_port_ctrl_t serial;   /** Serial port support */
            const can5_muxd_port_wiring_t mux;  /** Muxed port support */
            //const can5_pin_t le;                /** port latch enable (used to latch the EN signal before changing port) */
#if CONFIG_IDF_TARGET_ESP32
            const can5_pin_t en;
#endif
        } u;

        /** Net port support */
        struct {
            can5_serial_port_ctrl_t serial;   /** Serial port support */
            const can5_pin_t en;   // perst
        } net;

    } config;

    can5_port_idx_t selctd;

} can5_port_hdl_t;

/**
 * @brief User button status
 */
typedef enum can5_btn_status_e {
    CAN5_BTN_STAT_INITD,
    CAN5_BTN_STAT_PRESS,
    CAN5_BTN_STAT_LONG_PRESS,
} can5_btn_status_t;

/*
 * @brief User button peripheral
 */
typedef struct can5_button_s {
    volatile can5_btn_status_t status;
    const can5_pin_t pin;
    int64_t last_press;
    int press_count;
    task_t task;
} can5_btn_t;

typedef enum can5_wifi_status_e {
    CAN5_WIFI_STAT_UNINITD,
    CAN5_WIFI_STAT_INITD,
    CAN5_WIFI_STAT_CONNECTING,
    CAN5_WIFI_STAT_CONNECTED,
    CAN5_WIFI_STAT_COUNT,
} can5_wifi_status_t;

/**
 * @brief Wifi peripheral
 */
typedef struct can5_wifi_s {
    volatile can5_wifi_status_t status;
    struct {
        esp_netif_t *netif;
        wifi_config_t config;
        EventGroupHandle_t event_group;
    } sta;
    struct {
        esp_netif_t *netif;
        wifi_config_t config;
    } ap;
    esp_event_handler_instance_t evt_wifi;                          // sta start
    esp_event_handler_instance_t evt_got_ip;                        // got ip
    char sta_ip[IP4ADDR_STRLEN_MAX];
    esp_timer_handle_t connect_timer;
    int connect_retries;
} can5_wifi_t;

/**
 * @brief Cellular
 */

typedef struct can5_cell_s {
    EventGroupHandle_t evt_grp;
    esp_netif_t *cell_netif;
    esp_modem_dce_t *cell_dce;
    char cell_ip[IP4ADDR_STRLEN_MAX];
    esp_timer_handle_t connect_timer;
    int connect_retries;
} can5_cell_t;

/**
 * @brief SNTP
 */

typedef struct can5_sntp_s {
    bool enable_sntp;
    bool started;
    char sntp_address[32];                                          // sntp address
} can5_sntp_t;

/**
 * @brief SD MMC
 */

typedef struct can5_sdmmc_s {
    const char *mount_point;
#ifndef CAN5_LINUX_HOST_TEST
    sdmmc_card_t *card;
    sdmmc_host_t host;
#endif
} can5_sdmmc_t;



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
 * @brief  Get module status
 * 
 * @return int32_t status 
 */
static int32_t status_get();

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

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0                                                          

/**
 * @brief Return a string corresponding to the status descruption
 * 
 * @return const char* status description 
 */
static const char* status_getstr(int32_t status);

/**
 * @brief Return a string corresponding to the status descruption
 * 
 * @return const char* status description 
 */
static const char* evt_getstr(int32_t evt);

#endif

static can5_err_t enable        (bool enable, can5_port_idx_t port);
static bool       is_enabled    (can5_port_idx_t port);
static can5_err_t serial_recv   (void* pbuffer, size_t* plen, can5_port_idx_t port, uint16_t to_ms) ;
static can5_err_t serial_send   (const void* pdata, size_t len, can5_port_idx_t port, uint16_t to_ms);
static can5_err_t i2c_read_reg  (uint8_t addr7, uint8_t reg, uint8_t* dataout, size_t len, i2c_ack_type_t ack_type);
static can5_err_t i2c_write_reg (uint8_t addr7, uint8_t reg, uint8_t* data, size_t len, bool ack_check);
static bool       digital_read  (can5_port_idx_t port) ;
static can5_err_t digital_write (bool value, can5_port_idx_t port);
static can5_err_t analog_read   (uint16_t* const psample, can5_port_idx_t port) ;
static can5_err_t analog_write  (uint16_t pwm, can5_port_idx_t port);
static can5_err_t reset_netport1();
static can5_err_t serial_char_detect_install(char ch, can5_port_idx_t port, can5_serial_pattern_func cb);
static can5_err_t serial_char_detect_remove(can5_port_idx_t port);
static can5_err_t wifi_start(bool start_ap, bool start_sta, const char *sta_ssid, const char *sta_pass,
                             const char *ap_ssid, const char *ap_pass, bool enable_sntp, const char *sntp_server);
static can5_err_t cell_start(bool enable_net, bool enable_gps, const char *apn, bool enable_sntp, const char *sntp_server);
static can5_err_t get_cell_gps(char *out_str);
static int16_t get_cell_rssi();
static int16_t get_wifi_rssi();
static const char *get_ip_sta();
static const char *get_ip_cell();
static can5_err_t hal_sntp_start();
static can5_err_t hal_sntp_stop();
static can5_err_t shutdown_cb();
static can5_err_t get_mac_addresses(uint8_t *ap_mac, uint8_t *sta_mac);
static can5_err_t get_device_id(uint64_t *device_id);
static can5_err_t print_cell_status();
static can5_err_t get_wifi_sta_status(wifi_ap_record_t *record, bool *connected);

#ifdef CONFIG_IDF_TARGET_ESP32S3
static can5_err_t can6_enable_sens_switch(bool enable);
static can5_err_t can6_enable_net_switch(bool enable);

static can5_err_t can6_pm_enable(bool enable);
#endif


/**
 * @brief Initialize the UART from specified port config
 * 
 * @return can5_err_t 
 */
can5_err_t _serial_init(can5_serial_port_ctrl_t* pctrl);

/**
 * @brief Drive the multiplexer to select the right lines
 * 
 * @param pmux      pointer to struct containing the pin definition for the mux
 * @param selection number to write (mux i/o to activate)
 * @return can5_err_t 
 */
can5_err_t _mux_select(const can5_muxd_port_wiring_t* pmux, uint8_t selection);

/**
 * @brief Configure interrupt for net port
 * 
 * @return can5_err_t 
 */
can5_err_t _interrupts_and_sleep_init(void);

/**
 * @brief INterrupt handler
 * 
 * @param arg 
 */
//static void _hal_isr_handler(void* arg);
static void IRAM_ATTR _hal_isr_handler(void* arg);

/**
 * @brief Wifi event handler
 *
 * @return arg
 */
static void __wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
/**
 * @brief PPP event handler
 *
 * @return arg
 */
static void _ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data);
/**
 * @brief PPP ip event handler
 *
 * @return arg
 */
static void _ppp_ip_event(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data);
/**
 * @brief Init gpio for mux control pins
 * 
 * @param p_mux mux wiring structure pointer
 * @return can5_err_t 
 */

can5_err_t _mux_ctrl_init(const  can5_muxd_port_wiring_t* p_mux);

/**
 * @brief 
 * 
 * @param pvParameters 
 */
static portTASK_FUNCTION(_uart_event_task, pvParameters);

/**
 * @brief Sart i2c driver
 * 
 * @return esp_err_t 
 */
static esp_err_t _i2c_master_init(void);

#if CONFIG_IDF_TARGET_ESP32
/**
 * @brief Init adc1
 * 
 * @return esp_err_t 
 */
static esp_err_t _adc_init(void);
#endif


/**
 * @brief Init sd card
 *
 * @return esp_err_t
 */
static esp_err_t _sdmmc_init(void);


/**
 * @brief Uninitialize sd card
 *
 * @return esp_err_t
 */
static esp_err_t _sdmmc_deinit(void);

/**
 * @brief Init nvs
 *
 * @return esp_err_t
 */
static esp_err_t _nvs_init(void);

/**
 * @brief Button task
 *
 * @param pvParameters
 */
static portTASK_FUNCTION(_btn_event_task, pvParameters);

static void __wifi_connect_timer(void *args);

static uint64_t __wifi_get_next_connect();

static void __cell_connect_timer(void *args);

static uint64_t __cell_get_next_connect();

static esp_err_t _bms_init(void);

#ifdef CONFIG_IDF_TARGET_ESP32S3
static esp_err_t _can6_special_gpio_init(void);

static esp_err_t can6_status_led(const bool enable);

#endif
//_______________________________________________________________________________________________________
//
//   VARIABLES
//-------------------------------------------------------------------------------------------------------


/**
 * @brief Module available functions
 * 
 */
can5_hal_t hal = {

    .module = {
        .init = init,
        .uninit = uninit,
        .pre_sleep = pre_sleep,
        .post_sleep = post_sleep,
        .is_sleepable = is_sleepable,
        .is_sleeping = is_sleeping,
        .status_get = status_get,
#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0
        .status_getstr = status_getstr,
        .evt_getstr = evt_getstr,
#endif
    },

    .enable        = enable,
    .is_enabled    = is_enabled,
    .serial_send   = serial_send  ,
    .serial_recv   = serial_recv  ,
    .i2c_write_reg = i2c_write_reg,
    .i2c_read_reg  = i2c_read_reg,
    .digital_read  = digital_read ,
    .digital_write = digital_write,
    .analog_read   = analog_read  ,
    .analog_write  = analog_write ,
    .reset_netport1 = reset_netport1,
    .serial_char_detect_install = serial_char_detect_install,
    .serial_char_detect_remove = serial_char_detect_remove,
    .wifi_start = wifi_start,
    .cell_start = cell_start,
    .get_cell_rssi = get_cell_rssi,
    .get_wifi_rssi = get_wifi_rssi,
    .get_cell_gps = get_cell_gps,
    .get_ip_sta = get_ip_sta,
    .get_ip_cell = get_ip_cell,
    .sntp_start = hal_sntp_start,
    .sntp_stop = hal_sntp_stop,
    .shutdown_cb = shutdown_cb,
    .get_mac_addresses = get_mac_addresses,
    .get_device_id = get_device_id,
    .print_cell_status = print_cell_status,
    .get_wifi_sta_status = get_wifi_sta_status,
#ifdef CONFIG_IDF_TARGET_ESP32S3
    .enable_sens_switch = can6_enable_sens_switch,
    .enable_net_switch = can6_enable_net_switch,
    .pm_enable =  can6_pm_enable,
    .set_status_led = can6_status_led,
#endif
};

static struct hal_hdl_s {
    can5_hal_status_t status;
    bool enable[CAN5_PORT_COUNT];
    can5_port_hdl_t netport;
#if CONFIG_IDF_TARGET_ESP32
    can5_port_hdl_t adport;
#endif
    can5_port_hdl_t uport;
    const can5_pin_t netrst; // netport reset
    const can5_pin_t wake; // interrupt (wake)
    can5_btn_t usrbtn;
    //const can5_pin_t usrbtn; // interrupt (user button)
    const can5_pin_t rtcint; // interrupt (rtc)
    const can5_pin_t bsmint; // interrupt (battery monitoring)
    esp_adc_cal_characteristics_t adc_chars;
    can5_wifi_t wifi;
    can5_sdmmc_t sdmmc;
    can5_cell_t cell;
    can5_sntp_t sntp;
} __hhal = {
    .status = HAL_STAT_UNINITD,
    .netrst = CAN5_PIN_NETPORT_PERST,
    //.usrbtn = CAN5_PIN_USRBTN,
    .usrbtn = {
        .status = CAN5_BTN_STAT_INITD,
        .pin = CAN5_PIN_USRBTN,
        .last_press = 0,
        .press_count = 0,
    },
    .rtcint =  CAN5_PIN_RTCINT,
    .bsmint =  CAN5_PIN_BSMINT,
    .wake  = CAN5_PIN_WAKE,
    .netport = {
        .type   = CAN5_PORT_NET,
        .selctd = NETPORT_1,
        .config.net = {
            .en = CAN5_PIN_NETPORT_PERST,
            .serial = {
                .uart  = CAN5_UART_NETPORT,
                .tx    = CAN5_PIN_NETPORT_TX,
                .rx    = CAN5_PIN_NETPORT_RX,
                .tx_bufsz = CAN5_NETPORT_TX_BUFSZ,
                .rx_bufsz = CAN5_NETPORT_RX_BUFSZ,
                .uart_config = {
                    .baud_rate = CAN5_NETPORT_UART_BAUDRATE,
                    .data_bits = UART_DATA_8_BITS,
                    .parity = UART_PARITY_DISABLE,
                    .stop_bits = UART_STOP_BITS_1,
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if CONFIG_IDF_TARGET_ESP32
                    .source_clk = UART_SCLK_REF_TICK,
#endif
                },
                .pattern_installed = CAN5_PORT_NULL,
            },
            
        }
    },
    .uport = {
        .type   = CAN5_PORT_U,
        .selctd = CAN5_PORT_NULL,
        .config.u ={
#if CONFIG_IDF_TARGET_ESP32
            .en = CAN5_PIN_UPORT_EN,
#endif
            .serial = {
                .uart  = CAN5_UART_UPORT,
                .tx    = CAN5_PIN_UPORT_TX,
                .rx    = CAN5_PIN_UPORT_RX,
                .tx_bufsz = CAN5_UPORT_TX_BUFSZ,
                .rx_bufsz = CAN5_UPORT_RX_BUFSZ,
                .uart_config = {
                    .baud_rate = CAN5_UPORT_UART_BAUDRATE,
                    .data_bits = UART_DATA_8_BITS,
                    .parity    = UART_PARITY_DISABLE,
                    .stop_bits = UART_STOP_BITS_1,
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if CONFIG_IDF_TARGET_ESP32
                    .source_clk = UART_SCLK_REF_TICK,
#endif
                },
                .pattern_installed = CAN5_PORT_NULL,
            },
            .mux = {
                .inhib = CAN5_PIN_PORT_MUX_INH,
                .selct = {
                    CAN5_PIN_UPORT_SEL0,
                    CAN5_PIN_UPORT_SEL1,
#if CONFIG_IDF_TARGET_ESP32
                    CAN5_PIN_UPORT_SEL2
#endif
                },
#if CONFIG_IDF_TARGET_ESP32
                .selct_size = 3
#elif CONFIG_IDF_TARGET_ESP32S3
                .selct_size = 2
#endif
            },
        }
    },
#if CONFIG_IDF_TARGET_ESP32
    .adport = {
        .type   = CAN5_PORT_AD,
        .selctd = CAN5_PORT_NULL,
        .config.ad = {
            .en = CAN5_PIN_ADPORT_EN,
            .mux = {
                .inhib = CAN5_PIN_PORT_MUX_INH,
                .selct = {
                    CAN5_PIN_ADPORT_SEL0,
                    CAN5_PIN_ADPORT_SEL1,
                },
                .selct_size = 2
            },
        },
    },
#endif
    .wifi = {
        .status = CAN5_WIFI_STAT_UNINITD,
        .sta = {
            .netif = NULL,
            .config = {
                .sta = {
                    .threshold.authmode = WIFI_AUTH_OPEN,
                    .channel = 0,
                    .password = "",
                    //.pmf_cfg = {
                    //    .capable = true,
                    //    .required = false
                    //},
                }
            },
        },
        .ap = {
            .netif = NULL,
            .config = {
                .ap = {
                    .ssid = "",
                    .ssid_len = 0,
                    .channel = 11,
                    .password = "",
                    .max_connection = 3,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                },
            },
        },
        .sta_ip = { 0 },
        .connect_retries = 0,
        .connect_timer = NULL,
    },

    .sdmmc = {
        .mount_point = CAN5_FATFS_MOUNT_DIR,
    },

    .cell = {
        .cell_dce = NULL,
        .cell_netif = NULL,
        .evt_grp = NULL,
        .cell_ip = { 0 },
    },

    .sntp = {
        .enable_sntp = false,
        .sntp_address = { 0 },
        .started = false,
    }
};


ESP_EVENT_DEFINE_BASE(CAN5_EVT_HAL);


//_______________________________________________________________________________________________________
//
//   FUNCTIONS DEFNITION
//-------------------------------------------------------------------------------------------------------



static can5_err_t init() {

    if (__hhal.status == HAL_STAT_INITD) return CAN5_SUCCESS;

    ESP_LOGI(TAG, "serial");

    
    // init u serial
    VERIFY_SUCCESS(_serial_init(&__hhal.uport.config.u.serial));

    ESP_LOGI(TAG, "interrupts and sleep");
    // init interrupts
    VERIFY_SUCCESS(_interrupts_and_sleep_init());

#if CONFIG_IDF_TARGET_ESP32
    VERIFY_SUCCESS(_mux_ctrl_init(&__hhal.adport.config.ad.mux));
#endif
    VERIFY_SUCCESS(_mux_ctrl_init(&__hhal.uport.config.u.mux));


    VERIFY_SUCCESS(_i2c_master_init());

#if CONFIG_IDF_TARGET_ESP32
    VERIFY_SUCCESS(_adc_init());
#endif

    VERIFY_SUCCESS(_nvs_init());

    VERIFY_SUCCESS(_sdmmc_init());

    VERIFY_SUCCESS(_bms_init());

#if CONFIG_IDF_TARGET_ESP32S3
    VERIFY_SUCCESS(_can6_special_gpio_init());
#endif
#if CONFIG_CAN5_HAL_SNTP_TIME_SYNC && LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);
#endif

    // config common outputs
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type  = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19

#if CONFIG_IDF_TARGET_ESP32
    io_conf.pin_bit_mask = BIT64(__hhal.uport.config.u.en) | BIT64(__hhal.adport.config.ad.en); // | BIT64(__hhal.uport.config.u.le) | BIT64(__hhal.adport.config.ad.le);
#elif CONFIG_IDF_TARGET_ESP32S3
    io_conf.pin_bit_mask = BIT64(CAN6_PIN_PM_EN);
#endif

    gpio_output_set(0, io_conf.pin_bit_mask, io_conf.pin_bit_mask, 0);


    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    VERIFY_SUCCESS(gpio_config(&io_conf));


    // the following does not require gpio_output_set?

    gpio_config_t netrst = {
        .pin_bit_mask = BIT64(__hhal.netrst),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    gpio_output_set(netrst.pin_bit_mask, 0 , netrst.pin_bit_mask, 0);
    VERIFY_SUCCESS(gpio_config(&netrst));

    /** TODO: initialize Analog pins from configuration settings*/
    __hhal.status = HAL_STAT_INITD;
    esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_INITIALIZED, NULL, 0, pdMS_TO_TICKS(1000));

    return CAN5_SUCCESS;
}



static can5_err_t uninit() {
    CHECK_INITD();
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    __hhal.status = HAL_STAT_UNINITD;
    return CAN5_SUCCESS;
}

#if CONFIG_IDF_TARGET_ESP32
static esp_err_t _adc_init(void)
{
    //Configure ADC

    VERIFY_SUCCESS(adc1_config_width(ADC_WIDTH_BIT_12));
    VERIFY_SUCCESS(adc1_config_channel_atten(CAN5_ADC_CHANNEL, ADC_ATTEN_DB_11));  // Pin 33

    //Characterize ADC
    CLEAR_STRUCT(__hhal.adc_chars);
    return esp_adc_cal_characterize(CAN5_ADC_UNIT, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &__hhal.adc_chars);


}
#endif


static esp_err_t _i2c_master_init(void)
{
    int i2c_master_port = CAN5_I2C_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = CAN5_PIN_SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = CAN5_PIN_SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE ;
    conf.master.clk_speed = CAN5_I2C_SPEED;
    conf.clk_flags = 0;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

static esp_err_t _sdmmc_init(void)
{
    esp_err_t ret;

#if CONFIG_IDF_TARGET_ESP32
    gpio_set_pull_mode(CAN5_PIN_SDSPI_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CAN5_PIN_SDSPI_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CAN5_PIN_SDSPI_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CAN5_PIN_SDSPI_CS, GPIO_PULLUP_ONLY);
#endif

#ifndef CAN5_LINUX_HOST_TEST

    ESP_LOGI(TAG, "Initializing SD card SPI");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 8,
            .allocation_unit_size = 16 * 1024

    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    __hhal.sdmmc.host = host;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CAN5_PIN_SDSPI_MOSI,
        .miso_io_num = CAN5_PIN_SDSPI_MISO,
        .sclk_io_num = CAN5_PIN_SDSPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092
    };

    ret = spi_bus_initialize(__hhal.sdmmc.host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize spi bus.");
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CAN5_PIN_SDSPI_CS;
    slot_config.host_id = __hhal.sdmmc.host.slot;

    ret = esp_vfs_fat_sdspi_mount(__hhal.sdmmc.mount_point,
                                  &__hhal.sdmmc.host, &slot_config, &mount_config,
                                  &__hhal.sdmmc.card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). ", esp_err_to_name(ret));
        }
        return ret;
    }

    sdmmc_card_print_info(stdout, __hhal.sdmmc.card);
    ESP_LOGI(TAG, "SD card mounted.");
#endif

    return ret;
}

static esp_err_t _sdmmc_deinit(void) {
    esp_vfs_fat_sdcard_unmount(__hhal.sdmmc.mount_point, __hhal.sdmmc.card);
    ESP_LOGI(TAG, "SD card unmounted.");
    return CAN5_SUCCESS;
}

static esp_err_t _nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        CAN5_ERR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

static esp_err_t _bms_init(void)
{

    return CAN5_SUCCESS;
}


#ifdef CONFIG_IDF_TARGET_ESP32S3
static esp_err_t _can6_special_gpio_init(void)
{

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type  = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(CAN6_PIN_LED_STATUS) | BIT64(CAN6_PIN_SEN_PWR_EN) |
        BIT64(CAN6_PIN_NET_PWR_EN) | BIT64(CAN6_PIN_RGB_GPIO);


    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    VERIFY_SUCCESS(gpio_config(&io_conf));

    return CAN5_SUCCESS;
}



static esp_err_t can6_status_led(const bool enable)
{
    gpio_set_level(CAN6_PIN_LED_STATUS, enable);
    return CAN5_SUCCESS;
}

static esp_err_t can6_enable_sens_switch(const bool enable)
{
    gpio_set_level(CAN6_PIN_SEN_PWR_EN, enable);
    vTaskDelay(pdMS_TO_TICKS(100));
    return CAN5_SUCCESS;
}

static esp_err_t can6_enable_net_switch(const bool enable)
{
    gpio_set_level(CAN6_PIN_NET_PWR_EN, enable);
    vTaskDelay(pdMS_TO_TICKS(1000));
    return CAN5_SUCCESS;
}

static esp_err_t can6_pm_enable(const bool enable)
{
    gpio_set_level(CAN6_PIN_PM_EN, enable);
    return CAN5_SUCCESS;
}

#endif

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

static int32_t status_get() {
    return (int32_t)__hhal.status;
}

//   PORT OPERATIONS
//-------------------------------------------------------------------------------------------------------

can5_err_t _mux_select(const can5_muxd_port_wiring_t* pmux, uint8_t selection) {

    VERIFY_NOT_NULL(pmux);
    
    assert(pmux->selct_size <= CAN5_MUX_SEL_BITS_MAX);
    for (int bit=0; bit<pmux->selct_size; bit++) {
        ESP_LOGI_V(TAG, "mux set bit %d [pin %d]: %d", bit, pmux->selct[bit], GET_BIT(bit, selection));
        gpio_set_level(pmux->selct[bit], GET_BIT(bit, selection)); // set pin mux_sel[s]  <- GET_BIT(s, port_offset)
    }

    return CAN5_SUCCESS;
}

static bool is_enabled(can5_port_idx_t port) { 
    return __hhal.enable[port];
}

static can5_err_t enable(bool enable, can5_port_idx_t port) {
    CHECK_INITD();
    CHECK_VALID_PORT(port);

    // if already enabled, nothing to do
    //if (__hhal.enable[port] == enable) return CAN5_SUCCESS;

    can5_port_type_t typ = PORT_TYP_GET(port);
    can5_port_hdl_t* phdl = PORT_TYP_HDL_GETP(typ);
    const can5_muxd_port_wiring_t* pmux;

#ifdef CONFIG_IDF_TARGET_ESP32
    can5_pin_t  en;
#endif
    uint32_t  port_offset;

    switch (typ) {

        case CAN5_PORT_NET:
            // if net is port is enabled, that implies can5_hal handles the read, write.
            if (!__hhal.enable[port]) {
                ESP_LOGI(TAG, "netport selected, now managed by can5_hal.");
                __hhal.enable[port] = true;
                return _serial_init(&__hhal.netport.config.net.serial);
            }
            else {
                ESP_LOGD(TAG, "netport selected, nothing to do");
                return CAN5_SUCCESS;
            }
#if CONFIG_IDF_TARGET_ESP32
        case CAN5_PORT_AD:  

            // get the port number for the specific port type
            port_offset = port - ADPORT_0;
            pmux = &phdl->config.ad.mux;
            en = phdl->config.ad.en;
            //le = phdl->config.ad.le;
            break;
#endif

        case CAN5_PORT_U:  // mux for U has 3 bits select
            // get the port number for the specific port type
            port_offset = port - UPORT_0;
            pmux = &phdl->config.u.mux;
#ifdef CONFIG_IDF_TARGET_ESP32
            en = phdl->config.u.en;
#endif
            //le = phdl->config.u.le;
            break;
        
        default:
            ESP_LOGE(TAG, "port out-of-bound");
            return CAN5_HAL_ERR_INVALID_PORT;
    }
    

    // 2) inhibit mux
    /* inhibit */
    ESP_LOGD(TAG, "mux inhibit [pin %d]",pmux->inhib);
    gpio_set_level(pmux->inhib, 1); //
    vTaskDelay(MUX_INHIB_DELAY);

    /** switch port if not already selected */
    if (phdl->selctd != port) {

        // 3) set mux select bits
        VERIFY_SUCCESS(_mux_select(pmux, port_offset));
        phdl->selctd = port;
        vTaskDelay(MUX_SELECT_DELAY);


    }

    // 3) set the data value (en signal)
    // phdl->config.u.wiring.en <- __hhal.enable[port];
#ifdef CONFIG_IDF_TARGET_ESP32
    gpio_set_level(en, enable ? 1 : 0);// CAN5_UPORT_PIN_EN <- enable;
#endif

    // 4) re eanble mux if it wasn't
    // phdl->config.u.wiring.mxinhib <- 0
    ESP_LOGD(TAG, "mux enable [pin %d]",pmux->inhib);
    gpio_set_level(pmux->inhib, 0); //
    vTaskDelay(MUX_INHIB_DELAY);

    __hhal.enable[port] = enable;

    return CAN5_SUCCESS;
}

static can5_err_t __serial_receive_internal(can5_serial_port_ctrl_t* pctrl, void* pbuffer, size_t* plen, uint16_t to_ms) {

    /*
    if (__is_pattern_detect_active(pctl)) {
        return CAN5_HAL_ERR_PATTERN_ENABLED;
    }
    */

    TickType_t to = pdMS_TO_TICKS(to_ms);
    to = to ? to : 1;
    int ret = -1;

    ret = uart_read_bytes(pctrl->uart, (uint8_t*)pbuffer, *plen, to);
    // release mutex

    if (ret < 0) {
        *plen = 0;
        ESP_LOGE(TAG, "hal serial_recv error");
        return CAN5_HAL_ERR_SERIAL_RECV;
    } else if (ret == 0) {
        *plen = 0;
        return CAN5_HAL_ERR_TIMEOUT;
    }
    *plen = ret;

    return CAN5_SUCCESS;
}


static can5_err_t serial_recv   (void* pbuffer, size_t* plen, can5_port_idx_t port, uint16_t to_ms) {
    CHECK_INITD();
    can5_serial_port_ctrl_t* const pctrl = PORT_SERIAL_CONFIG_GETP(port);
    if(pctrl == NULL) return CAN5_HAL_ERR_INVALID_PORT; // retrun error  -is not a serial port

    can5_port_type_t typ = PORT_TYP_GET(port);
    can5_port_hdl_t* phdl = PORT_TYP_HDL_GETP(typ);
    const can5_muxd_port_wiring_t* pmux;

#ifdef CONFIG_IDF_TARGET_ESP32
    can5_pin_t  en;
#endif

    uint32_t  port_offset;


    switch (typ) {

        case CAN5_PORT_NET:  
            return __serial_receive_internal(pctrl, pbuffer, plen, to_ms);

        case CAN5_PORT_U:  // mux for U has 3 bits select
            // get the port number for the specific port type
            port_offset = port - UPORT_0;
#ifdef CONFIG_IDF_TARGET_ESP32
            en = phdl->config.u.en;
#endif
            pmux = &phdl->config.u.mux;
            break;
        
        default:
            ESP_LOGE(TAG, "port out-of-bound");
            return CAN5_HAL_ERR_INVALID_PORT;
    }
    

    /** switch port if not already selected */
    if (phdl->selctd != port) {

        // 1) inhibit mux
        /* inhibit */
        ESP_LOGD(TAG, "mux inhibit [pin %d]",pmux->inhib);
        gpio_set_level(pmux->inhib, 1); //
        vTaskDelay(MUX_INHIB_DELAY);

        // 2) set mux select bits
        VERIFY_SUCCESS(_mux_select(pmux, port_offset));
        phdl->selctd = port;
        vTaskDelay(MUX_SELECT_DELAY);

        // 3) restore the right enable value
        // phdl->config.u.wiring.en <- __hhal.enable[port];
#ifdef CONFIG_IDF_TARGET_ESP32
        gpio_set_level(en, __hhal.enable[port]);// CAN5_UPORT_PIN_EN <- enable;
#endif

        // 3.5) clear uart rx buffer to remove residue of previous port.
        //uart_flush(phdl->config.u.serial.uart);


        // 4) re eanble mux if it wasn't
        // phdl->config.u.wiring.mxinhib <- 0
        gpio_set_level(pmux->inhib, 0); //
        vTaskDelay(MUX_INHIB_DELAY);
    }

    // 5) receive data
    return __serial_receive_internal(pctrl, pbuffer, plen, to_ms);
}

static can5_err_t serial_send   (const void* pdata, size_t len, can5_port_idx_t port, uint16_t timeout_ms) {
    CHECK_INITD();
    can5_serial_port_ctrl_t* pctrl = PORT_SERIAL_CONFIG_GETP(port);
    if(pctrl == NULL) return CAN5_HAL_ERR_INVALID_PORT; // retrun error  -is not a serial port


    can5_port_type_t typ = PORT_TYP_GET(port);
    can5_port_hdl_t* phdl = PORT_TYP_HDL_GETP(typ);
    const can5_muxd_port_wiring_t* pmux;

#ifdef CONFIG_IDF_TARGET_ESP32
    can5_pin_t  en;
#endif

    uint32_t  port_offset;

    TickType_t to = pdMS_TO_TICKS(timeout_ms);
    int ret;
    switch (typ) {

        case CAN5_PORT_NET:  
            ESP_LOGD(TAG, "netport selected, nothing to do");
            ret = uart_write_bytes(pctrl->uart, (const char*)pdata, len);
            return to ? uart_wait_tx_done(pctrl->uart, to) : ((ret==len)?CAN5_SUCCESS:CAN5_HAL_ERR_SERIAL_SEND);

#if CONFIG_IDF_TARGET_ESP32
        case CAN5_PORT_AD:  
            return CAN5_HAL_ERR_INVALID_PORT; 
            break;
#endif

        case CAN5_PORT_U:  // mux for U has 3 bits select
            // get the port number for the specific port type
            port_offset = port - UPORT_0;
#ifdef CONFIG_IDF_TARGET_ESP32
            en = phdl->config.u.en;
#endif
            pmux = &phdl->config.u.mux;
            break;
        
        default:
            ESP_LOGE(TAG, "port out-of-bound");
            return CAN5_HAL_ERR_INVALID_PORT;
    }
    

    /** switch port if not already selected */
    if (phdl->selctd != port) {

        // 1) inhibit mux
        /* inhibit */
        ESP_LOGD(TAG, "mux inhibit [pin %d]",pmux->inhib);
        gpio_set_level(pmux->inhib, 1); //
        vTaskDelay(MUX_INHIB_DELAY);

        // 2) set mux select bits
        VERIFY_SUCCESS(_mux_select(pmux, port_offset));
        phdl->selctd = port;
        vTaskDelay(MUX_SELECT_DELAY);

        // 3) restore the right enable value
        // phdl->config.u.wiring.en <- __hhal.enable[port];
#ifdef CONFIG_IDF_TARGET_ESP32
        gpio_set_level(en, __hhal.enable[port]);// CAN5_UPORT_PIN_EN <- enable;
#endif

        // 3.5) clear uart rx buffer to remove residue of previous port.
        //uart_flush(phdl->config.u.serial.uart);

        // 4) re eanble mux if it wasn't
        // phdl->config.u.wiring.mxinhib <- 0
        gpio_set_level(pmux->inhib, 0); //
        vTaskDelay(MUX_INHIB_DELAY);

    }



    // 5) send data
    ret = uart_write_bytes(pctrl->uart, (const char*)pdata, len);


    // wait the timeout before returning
    return to ? uart_wait_tx_done(pctrl->uart, to) : ((ret==len)?CAN5_SUCCESS:CAN5_HAL_ERR_SERIAL_SEND);
}

#define CHECK_I2C_RET   if (ret != CAN5_SUCCESS) goto done

static can5_err_t i2c_write_reg    (uint8_t addr7, uint8_t reg, uint8_t* data, size_t len, bool ack_check) {
    CHECK_INITD();
    can5_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    ret = i2c_master_start(cmd);
    CHECK_I2C_RET;

    ret = i2c_master_write_byte(cmd, addr7 << 1 | I2C_MASTER_WRITE, ack_check);
    CHECK_I2C_RET;

    ret = i2c_master_write_byte(cmd, reg, ack_check);
    CHECK_I2C_RET;

    ret = i2c_master_write(cmd, data, len, ack_check);
    CHECK_I2C_RET;

    ret = i2c_master_stop(cmd);
    CHECK_I2C_RET;

    ret = i2c_master_cmd_begin(CAN5_I2C_NUM, cmd, 1000 / portTICK_RATE_MS);
done:
    i2c_cmd_link_delete(cmd);
    return CAN5_SUCCESS;
}

static can5_err_t i2c_read_reg (uint8_t addr7, uint8_t reg, uint8_t* dataout, size_t len, i2c_ack_type_t ack_type)
{
    CHECK_INITD();
    can5_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    ret = i2c_master_start(cmd);
    CHECK_I2C_RET;

    ret = i2c_master_write_byte(cmd, addr7 << 1 | I2C_MASTER_WRITE, true);
    CHECK_I2C_RET;

    ret = i2c_master_write_byte(cmd, reg, true);
    CHECK_I2C_RET;

    ret = i2c_master_start(cmd);
    CHECK_I2C_RET;

    ret = i2c_master_write_byte(cmd, addr7 << 1 | I2C_MASTER_READ, true);
    CHECK_I2C_RET;

    ret = i2c_master_read(cmd, dataout, len, ack_type);
    CHECK_I2C_RET;

    ret = i2c_master_stop(cmd);
    CHECK_I2C_RET;

    ret = i2c_master_cmd_begin(CAN5_I2C_NUM, cmd, 1000/ portTICK_RATE_MS);
done:
    i2c_cmd_link_delete(cmd);
    return ret;
}

static bool       digital_read  (can5_port_idx_t port) {
#if CONFIG_IDF_TARGET_ESP32
    CHECK_INITD();
    CHECK_ADPORT(port);
#endif
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    return CAN5_SUCCESS;
}

static can5_err_t digital_write (bool value, can5_port_idx_t port) {
#if CONFIG_IDF_TARGET_ESP32
    CHECK_INITD();
    CHECK_ADPORT(port);
#endif
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    return CAN5_SUCCESS;
}

static can5_err_t analog_read  (uint16_t* const psample, can5_port_idx_t port) {
#if CONFIG_IDF_TARGET_ESP32
    CHECK_INITD();
    CHECK_ADPORT(port);

    can5_port_hdl_t* phdl = PORT_TYP_HDL_GETP(CAN5_PORT_AD);
    const can5_muxd_port_wiring_t* pmux;
    can5_pin_t  en;
    uint32_t  port_offset;

    // get the port number for the specific port type
    port_offset = port - ADPORT_0;
    en = phdl->config.ad.en;
    pmux = &phdl->config.ad.mux;

    /** switch port if not already selected */
    if (phdl->selctd != port) {

        // 1) inhibit mux
        /* inhibit */
        ESP_LOGD(TAG, "mux inhibit [pin %d]", pmux->inhib);
        gpio_set_level(pmux->inhib, 1); //
        vTaskDelay(MUX_INHIB_DELAY);

        // 2) set mux select bits
        VERIFY_SUCCESS(_mux_select(pmux, port_offset));
        phdl->selctd = port;
        vTaskDelay(MUX_SELECT_DELAY);

        // 3) restore the right enable value
        // phdl->config.u.wiring.en <- __hhal.enable[port];
        gpio_set_level(en, __hhal.enable[port]);// CAN5_UPORT_PIN_EN <- enable;

        // 4) re eanble mux if it wasn't
        // phdl->config.u.wiring.mxinhib <- 0
        gpio_set_level(pmux->inhib, 0); //
        vTaskDelay(MUX_INHIB_DELAY);
    }
    
    *psample = esp_adc_cal_raw_to_voltage(adc1_get_raw((adc1_channel_t)CAN5_ADC_CHANNEL), &__hhal.adc_chars);
#endif

    return CAN5_SUCCESS;
}

static can5_err_t analog_write  (uint16_t pwm, can5_port_idx_t port) {
#if CONFIG_IDF_TARGET_ESP32
    CHECK_INITD();
    CHECK_ADPORT(port);
#endif
    ESP_LOGW(TAG, "%s Not implemented", __func__);
    return CAN5_SUCCESS;
}

static can5_err_t reset_netport1()
{
    gpio_set_level(__hhal.netrst, 0);

    if (__hhal.cell.cell_dce) {
        // For cell we have noticed that we need to keep pin at 0 longer
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else {
        vTaskDelay(pdMS_TO_TICKS(MUX_SELECT_DELAY));
    }
    gpio_set_level(__hhal.netrst, 1);
    return CAN5_SUCCESS;
}

can5_err_t _serial_init(can5_serial_port_ctrl_t* pctrl)  {
    /** 1) init hw */

    // if multimple overlapping uarts, use uart_is_driver_installed to check if alreaty initialized
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(pctrl->uart,
                                        &pctrl->uart_config));

    // configure pins
    // Set UART pins(TX: IO16 (UART2 default), RX: IO17 (UART2 default), RTS: IO18, CTS: IO19)
    ESP_ERROR_CHECK(uart_set_pin(pctrl->uart,   
                                    pctrl->tx,  
                                    pctrl->rx,  
                                    UART_PIN_NO_CHANGE,                    
                                    UART_PIN_NO_CHANGE)                    
    );

    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(pctrl->uart,  
                                        pctrl->tx_bufsz*2,       
                                        pctrl->rx_bufsz*2,       
                                        UART_EVENTS_QUEUE_SIZE,          
                                        &pctrl->uart_queue,     
                                        0)
    );

    return CAN5_SUCCESS;
}

static can5_err_t serial_char_detect_install(char ch, can5_port_idx_t port, can5_serial_pattern_func cb) {

    CHECK_SERIALPORT(port);
    can5_serial_port_ctrl_t* pctrl = PORT_SERIAL_CONFIG_GETP(port);

    if (__is_pattern_detect_active(pctl)) {
        ESP_LOGE(TAG, "Pattern detect already installed on uart %d, port %s", pctrl->uart, can5_hal_port_getstr(pctrl->pattern_installed));
        return CAN5_HAL_ERR_BUSY;
    }
    ESP_LOGD(TAG, "Enabling pattern detect on port: %s", can5_hal_port_getstr(port));

    //Set uart pattern detect function.
    VERIFY_SUCCESS(uart_enable_pattern_det_baud_intr(pctrl->uart, ch, 1, 9, 0, 0));
    //Reset the pattern queue length to record at most 20 pattern positions.
    VERIFY_SUCCESS(uart_pattern_queue_reset(pctrl->uart, 20));

    // Create a task to handler UART event from ISR
    static char tskname[25];
    snprintf(tskname, sizeof(tskname), "_uart_event_task");

    /** Support for uart events */
    if ((__hhal.netport.config.net.serial.pattern_installed != CAN5_PORT_NULL) || (__hhal.uport.config.u.serial.pattern_installed  != CAN5_PORT_NULL)) {
        ESP_LOGD(TAG, "Pattern Detect Task already running...");
        pctrl->pattern_installed = port;
        return CAN5_SUCCESS;
    } else {
        pctrl->pattern_installed = port;
        ESP_LOGD(TAG, "Creating uart pattern Detect Task : \"%s\"", tskname);
        pctrl->pattern_cb = cb;
        xTaskCreate(_uart_event_task, tskname, 2048, &__hhal, 12, NULL);
    }


    return CAN5_SUCCESS; 
}

static can5_err_t serial_char_detect_remove(can5_port_idx_t port) {

    CHECK_SERIALPORT(port);
    can5_serial_port_ctrl_t* pctrl = PORT_SERIAL_CONFIG_GETP(port);

    //Set uart pattern detect function.
    VERIFY_SUCCESS(uart_disable_pattern_det_intr(pctrl->uart));
    pctrl->pattern_installed = CAN5_PORT_NULL;

    return CAN5_SUCCESS; 
}

static struct {
    bool enable_gps;
    bool enable_net;
    char operator[64];
    char out[64];
} _cell_init_params;

#define CELL_RETIRES    16
static can5_err_t _cell_init(void *pvParameters)
{
    can5_err_t err;
    int retries, mode, rssi, ber;

    err = ESP_ERR_INVALID_STATE;

    hal.reset_netport1();

    vTaskDelay(pdMS_TO_TICKS(6000));

    retries = 0;
    // wait for AT to work again
    while ((err = esp_modem_sync(__hhal.cell.cell_dce) != ESP_OK)) {
        CAN5_ERR_CHECK_NO_ABORT(err);
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_modem_at(__hhal.cell.cell_dce, "ATH\r", NULL, 1000);
        // 1 minute
        if (++retries > CELL_RETIRES) {
            ESP_LOGE(TAG, "Modem sync exceed retires!");
            goto error;
        }
    }

    // if cell is off but gps is on
    if (!_cell_init_params.enable_net & _cell_init_params.enable_gps) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        CLEAR_ARRAY(_cell_init_params.out);
        err = esp_modem_at(__hhal.cell.cell_dce, "AT+CGPS=1", (char *) &_cell_init_params.out, 1000);
        CAN5_ERR_CHECK_NO_ABORT(err);
        ESP_LOGI(TAG, "Modem AT+CGPS=%s", _cell_init_params.out);

        return CAN5_SUCCESS;
    }

    /* TODO: Follow the following application note.
     * https://www.cika.com/soporte/Information/GSMmodules/Quectel/AppNotes/Quectel_GSM_PPP_Application_Note.pdf
     */

    mode = 0;
    retries = 0;
    // wait for GPRS+ signal
    while(err != ESP_OK && (mode < 2)) {
        err = esp_modem_get_network_system_mode(__hhal.cell.cell_dce, &mode);
        vTaskDelay(pdMS_TO_TICKS(500));
        if (++retries > CELL_RETIRES) {
            ESP_LOGE(TAG, "Modem sync exceed retires!");
            goto error;
        }
    }


    err = esp_modem_get_signal_quality(__hhal.cell.cell_dce, &rssi, &ber);
    CAN5_ERR_CHECK(err);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d", err);
    }

    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);

    vTaskDelay(pdMS_TO_TICKS(3000));
    err = esp_modem_get_operator_name(__hhal.cell.cell_dce, _cell_init_params.operator, &rssi);
    CAN5_ERR_CHECK_NO_ABORT(err);
    ESP_LOGI(TAG, "Operator: %s", _cell_init_params.operator);
    if (err == CAN5_SUCCESS) {
        esp_event_post(CAN5_EVT_HAL,
                       CAN5_HAL_EVT_CELL_OPERATOR,
                       (void *)_cell_init_params.operator,
                       strlen(_cell_init_params.operator) + 1, // include /0 at end
                       portMAX_DELAY);
    }

    if (_cell_init_params.enable_gps) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        CLEAR_ARRAY(_cell_init_params.out);
        err = esp_modem_at(__hhal.cell.cell_dce, "AT+CGPS=1", (char *) &_cell_init_params.out, 1000);
        CAN5_ERR_CHECK_NO_ABORT(err);
        //ESP_LOGI(TAG, "OUT: %s", _cell_init_params.out);
    }
    else {
        vTaskDelay(pdMS_TO_TICKS(1000));
        CLEAR_ARRAY(_cell_init_params.out);
        err = esp_modem_at(__hhal.cell.cell_dce, "AT+CGPS=0", (char *) &_cell_init_params.out, 1000);
        CAN5_ERR_CHECK_NO_ABORT(err);
    }

    err = esp_modem_set_mode(__hhal.cell.cell_dce, ESP_MODEM_MODE_CMUX);
    CAN5_ERR_CHECK_NO_ABORT(err);

    return CAN5_SUCCESS;

error:
    __hhal.cell.connect_retries++;
    return esp_timer_start_once(__hhal.cell.connect_timer, __cell_get_next_connect());
}

static can5_err_t cell_start(bool enable_net, bool enable_gps, const char *apn, bool enable_sntp, const char *sntp_server)
{
    VERIFY_SUCCESS(esp_netif_init());

    VERIFY_SUCCESS(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
        &_ppp_ip_event, NULL));
    VERIFY_SUCCESS(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
        &_ppp_changed, NULL));

    esp_timer_create_args_t timer_args = {
        .callback = __cell_connect_timer,
        .arg = &__hhal.cell.connect_retries,
    };

    VERIFY_SUCCESS(esp_timer_create(&timer_args, &__hhal.cell.connect_timer));

    ESP_LOGI(TAG, "Cell Mode enable_net: %s enable_gps: %s enable_sntp: %s", boolean_get_str(enable_net),
        boolean_get_str(enable_gps), boolean_get_str(enable_sntp));

    __hhal.cell.evt_grp = xEventGroupCreate();
    VERIFY_NOT_NULL(__hhal.cell.evt_grp);

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = CAN5_PIN_NETPORT_TX;
    dte_config.uart_config.rx_io_num = CAN5_PIN_NETPORT_RX;
    dte_config.uart_config.cts_io_num = -1;
    dte_config.uart_config.rts_io_num = -1;


    /* Configure the DCE */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(apn);

    /* Configure the PPP netif */
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

    __hhal.cell.cell_netif = esp_netif_new(&netif_ppp_config);
    assert(__hhal.cell.cell_netif);

    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
    __hhal.cell.cell_dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600,
                                             &dte_config, &dce_config,
                                             
                                             __hhal.cell.cell_netif);

    strcpy((char *) __hhal.sntp.sntp_address, sntp_server);
    __hhal.sntp.enable_sntp = enable_sntp;

    _cell_init_params.enable_gps = enable_gps;
    _cell_init_params.enable_net = enable_net;

    return esp_timer_start_once(__hhal.cell.connect_timer, __cell_get_next_connect());
}

static can5_err_t get_cell_gps(char *out_str)
{
    if (!__hhal.cell.cell_netif) {
        return CAN5_HAL_ERR_INVALID_PORT;
    }

    VERIFY_SUCCESS(esp_modem_at(__hhal.cell.cell_dce, "AT+CGNSSINFO",out_str, 15000));
    ESP_LOGI_V(TAG, "%s", out_str);
    return CAN5_SUCCESS;
}

static can5_err_t wifi_start(bool start_ap, bool start_sta, const char *sta_ssid, const char *sta_pass, const char *ap_ssid,
           const char *ap_pass, bool enable_sntp, const char *sntp_server)
{
    if (__hhal.wifi.status != CAN5_WIFI_STAT_UNINITD) {
        return CAN5_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Wifi Mode AP: %s STA: %s enable_sntp: %s", boolean_get_str(start_ap), boolean_get_str(start_sta),
             boolean_get_str(enable_sntp));

    if (!start_sta && !start_ap) {
        return CAN5_SUCCESS;
    }

    VERIFY_SUCCESS(esp_netif_init());

    VERIFY_SUCCESS(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &__wifi_event_handler, NULL, &__hhal.wifi.evt_wifi));

    if (start_sta) {
        __hhal.wifi.sta.netif = esp_netif_create_default_wifi_sta();

        VERIFY_SUCCESS(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP,
            &__wifi_event_handler, NULL, &__hhal.wifi.evt_got_ip));
        esp_timer_create_args_t timer_args = {
            .callback = __wifi_connect_timer,
            .arg = &__hhal.wifi.connect_retries,
        };
        VERIFY_SUCCESS(esp_timer_create(&timer_args, &__hhal.wifi.connect_timer));
    }

    if (start_ap) {
        __hhal.wifi.ap.netif = esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    VERIFY_SUCCESS(esp_wifi_init(&cfg));

    VERIFY_SUCCESS(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    strcpy((char *) __hhal.wifi.sta.config.sta.ssid, sta_ssid);

    strcpy((char *) __hhal.wifi.ap.config.ap.ssid, ap_ssid);
    __hhal.wifi.ap.config.ap.ssid_len = strlen(ap_ssid);


    strcpy((char *) __hhal.sntp.sntp_address, sntp_server);
    __hhal.sntp.enable_sntp = enable_sntp;

    __hhal.wifi.ap.config.ap.max_connection = CONFIG_CAN5_AP_WIFI_MAX_CONNECTIONS;

    /* AP wifi config */
    if (strlen(ap_pass) == 0) {
        __hhal.wifi.ap.config.ap.authmode = WIFI_AUTH_OPEN;
    }
    else {
        strcpy((char *) __hhal.wifi.ap.config.ap.password, ap_pass);
    }

    if (strlen(sta_pass) == 0) {
        ESP_LOGI(TAG, "Connecting to open wifi!");
    }
    else {
        strcpy((char *) __hhal.wifi.sta.config.sta.password, sta_pass);
    }

    if (start_sta && start_ap) {
        VERIFY_SUCCESS(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }
    else if (start_ap) {
        VERIFY_SUCCESS(esp_wifi_set_mode(WIFI_MODE_AP));
    }
    else if (start_sta) {
        VERIFY_SUCCESS(esp_wifi_set_mode(WIFI_MODE_STA));
    }
    else {
        return CAN5_HAL_ERR_INVALID_WIFI_MODE;
    }

    if (start_ap) {
        VERIFY_SUCCESS(esp_wifi_set_config(WIFI_IF_AP,
                                           &__hhal.wifi.ap.config));
    }

    if (start_sta) {
        VERIFY_SUCCESS(esp_wifi_set_config(WIFI_IF_STA,
                                           &__hhal.wifi.sta.config));
    }

    __hhal.wifi.status = CAN5_WIFI_STAT_INITD;

    VERIFY_SUCCESS(esp_wifi_start());

    return CAN5_SUCCESS;
}

static void __check_for_uart_events(can5_port_type_t type) {
// Waiting for UART event.
    uart_event_t event;
    //size_t buffered_size;

    can5_serial_port_ctrl_t* pctrl = PORT_TYP_SERIAL_CONFIG_GETP(type);
    can5_serial_pattern_event_data_t pattern_data;

    if (xQueueReceive(pctrl->uart_queue, (void * )&event, portMAX_DELAY)) {
        //ESP_LOGI(TAG, "uart[%d] event: %d", pctrl->uart, event.type);
        switch(event.type) {
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full. EDIT: Remove the pattern detection from event handlers. */
            case UART_PATTERN_DET:
                //uart_get_buffered_data_len(pctrl->uart, &buffered_size);
                pattern_data.pattern_pos = uart_pattern_get_pos(pctrl->uart);
                //ESP_LOGD(TAG, "[UART %d][PATTERN DETECTED] pos: %d, buffered size: %d", pctrl->uart, pctrl->pattern_pos, buffered_size);
                if (pattern_data.pattern_pos == -1) {
                    uart_flush_input(pctrl->uart);
                } else {
                    pattern_data.port = PORT_TYP_HDL_GETP(type)->selctd;
                    if (pctrl->pattern_cb) pctrl->pattern_cb(&pattern_data);
                }
                break;
            //Others
            default:
                //ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
        }
    }
}

static int16_t get_cell_rssi()
{
    int rssi, snr;
    can5_err_t ret;

    if (!__hhal.cell.cell_netif) {
        return 0;
    }

    if ((ret = esp_modem_get_signal_quality(__hhal.cell.cell_dce,  &rssi, &snr)) != ESP_OK) {
        return 0;
    }
    return rssi;
}

static int16_t get_wifi_rssi()
{
    wifi_ap_record_t ap;

    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return 0;
    }

    return ap.rssi;

}

static const char *get_ip_sta()
{
    return __hhal.wifi.sta_ip;
}

static const char *get_ip_cell()
{
    return __hhal.cell.cell_ip;
}

static can5_err_t print_cell_status()
{
    int volt, bcs, bcl;
    if (__hhal.cell.cell_dce) {
        VERIFY_SUCCESS(esp_modem_get_battery_status(__hhal.cell.cell_dce, &volt, &bcs, &bcl));
        ESP_LOGI(TAG, "[Simcom] volt: %d bcs: %d bcl: %d", volt, bcs, bcl);
    }


    return CAN5_SUCCESS;
}

static can5_err_t get_wifi_sta_status(wifi_ap_record_t *record, bool *connected)
{

    *connected  = __hhal.wifi.status == CAN5_WIFI_STAT_CONNECTED;

    if (*connected) {
        esp_wifi_sta_get_ap_info(record);
    }

    return CAN5_SUCCESS;
}

void __sntp_time_sync_notification_cb(struct timeval *tv)
{
    char tmbuf[64];
    struct tm now_time;
    now_time = *localtime(&tv->tv_sec);
    strftime(tmbuf, sizeof tmbuf, "%d-%m-%Y %H:%M:%S", &now_time);
    ESP_LOGI(TAG, "SNTP SYNC: %s.%06ld", tmbuf, tv->tv_usec);
    rtc.sys_to_rtc();
}

static can5_err_t hal_sntp_start()
{
    ESP_LOGI(TAG, "SNTP started enable_sntp: %s sntp_started: %s", boolean_get_str(__hhal.sntp.enable_sntp),
             boolean_get_str(__hhal.sntp.started));
    if (__hhal.sntp.enable_sntp && !__hhal.sntp.started) {
        ESP_LOGI(TAG, "SNTP started.");
#if CONFIG_CAN5_HAL_SNTP_TIME_SYNC
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
#if LWIP_DHCP_GET_NTP_SRV && SNTP_MAX_SERVERS > 1
        sntp_setservername(1, __hhal.sntp.sntp_address);
#elif CONFIG_IDF_TARGET_ESP32S3
        sntp_setservername(0, __hhal.sntp.sntp_address);
#endif
        sntp_set_time_sync_notification_cb(__sntp_time_sync_notification_cb);
        sntp_init();
        sntp_enabled();
#endif
        __hhal.sntp.started = true;
    }
    return CAN5_SUCCESS;
}

static can5_err_t hal_sntp_stop()
{
    if (__hhal.sntp.enable_sntp && __hhal.sntp.started) {
#if CONFIG_CAN5_HAL_SNTP_TIME_SYNC
        sntp_stop();
#endif
        __hhal.sntp.started = false;
    }
    return CAN5_SUCCESS;
}

static can5_err_t shutdown_cb()
{
    if (__hhal.cell.cell_dce) {
        //ESP_ERROR_CHECK_WITHOUT_ABORT(esp_modem_reset(__hhal.cell.cell_dce));
        esp_modem_set_mode(__hhal.cell.cell_dce, ESP_MODEM_MODE_COMMAND);
        esp_modem_power_down(__hhal.cell.cell_dce);
        esp_modem_destroy(__hhal.cell.cell_dce);
    }

    if (__hhal.cell.cell_netif) {
        esp_netif_destroy(__hhal.cell.cell_netif);
    }

    ESP_LOGI(TAG, "Set mux inhibit to 0");
    // set inhibit pin to 0 for mux inhibit pin before going to reboot
    gpio_set_level(__hhal.uport.config.u.mux.inhib, 0);

#if CONFIG_IDF_TARGET_ESP32
    gpio_set_level(__hhal.adport.config.ad.mux.inhib, 0);
#endif

    _sdmmc_deinit();

    return CAN5_SUCCESS;
}

static can5_err_t get_mac_addresses(uint8_t *ap_mac, uint8_t *sta_mac)
{
    if (sta_mac) {
        VERIFY_SUCCESS(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA));
        //ESP_LOGI(TAG, "STA MAC: %s", sta_mac);
    }

    if (ap_mac) {
        VERIFY_SUCCESS(esp_read_mac(ap_mac, ESP_MAC_WIFI_SOFTAP));
        //ESP_LOGI(TAG, "AP MAC: %s", ap_mac);
    }


    return CAN5_SUCCESS;
}


static can5_err_t get_device_id(uint64_t *device_id)
{
    static char sta_mac[6];

    VERIFY_SUCCESS(get_mac_addresses(NULL, (uint8_t *)sta_mac));

    *device_id = 0;
    for (int i = 5; i >= 0; i--) {
        *device_id |= ((uint64_t )sta_mac[i]) << ((5 - i) * 8);
    }

    return CAN5_SUCCESS;
}

static portTASK_FUNCTION(_uart_event_task, pvParameters)
{
    struct hal_hdl_s* ___hhal = pvParameters;

    bool keep_active;

    keep_active = true;
    ESP_LOGI(TAG, "check for event TaskStarted");
    while (keep_active) {

        
        keep_active = false;
        if (___hhal->netport.config.net.serial.pattern_installed!=CAN5_PORT_NULL) {
            __check_for_uart_events(CAN5_PORT_NET);
            keep_active = true;
        }
        if (___hhal->uport.config.u.serial.pattern_installed!=CAN5_PORT_NULL) {
            __check_for_uart_events(CAN5_PORT_U);
            keep_active = true;
        }

        taskYIELD();

    }

    ESP_LOGE(TAG, "Removing uart pattern Detect Task");
    vTaskDelete(NULL);
}


can5_err_t _mux_ctrl_init(const can5_muxd_port_wiring_t* p_mux) {

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type  = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 0;
    for (int s  = 0; s<p_mux->selct_size; s++) {
        io_conf.pin_bit_mask |= BIT64(p_mux->selct[s]);
    }

    gpio_output_set(BIT64(p_mux->inhib), io_conf.pin_bit_mask, io_conf.pin_bit_mask, 0);

    io_conf.pin_bit_mask |= BIT64(p_mux->inhib);
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    VERIFY_SUCCESS(gpio_config(&io_conf));


    return CAN5_SUCCESS;
}


can5_err_t _interrupts_and_sleep_init(void) {

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;

    //bit mask of the pins that you want to set,e.g.GPIO18/19
    // @rmukhia: Not using wake pin for now, if you want to enable this pin, please only enable it when not floating.
    //io_conf.pin_bit_mask = BIT64(CAN5_PIN_WAKE) | BIT64(CAN5_PIN_USRBTN) | BIT64(CAN5_PIN_RTCINT);
    io_conf.pin_bit_mask = BIT64(CAN5_PIN_USRBTN) | BIT64(CAN5_PIN_RTCINT);

    //disable pull-down mode
    io_conf.pull_down_en = 0;

#ifdef CONFIG_IDF_TARGET_ESP32S3
    // for can6, enable pull-up mode to avoid floating input
    io_conf.pull_up_en = 1;
#else
    //disable pull-up mode
    io_conf.pull_up_en = 0;
#endif

    //configure GPIO with the given settings
    VERIFY_SUCCESS(gpio_config(&io_conf));
    
    //install gpio isr service
    VERIFY_SUCCESS(gpio_install_isr_service(0));

    //hook isr handler for specific gpio pin
    // @rmukhia: Not using wake pin for now
    //VERIFY_SUCCESS(gpio_isr_handler_add(__hhal.wake, _hal_isr_handler, (void*) __hhal.wake));


    VERIFY_SUCCESS(gpio_isr_handler_add(__hhal.usrbtn.pin, _hal_isr_handler, (void*) __hhal.usrbtn.pin));
    //VERIFY_SUCCESS(gpio_isr_handler_add(__hhal.rtcint, _hal_isr_handler, (void*) __hhal.rtcint));

    VERIFY_SUCCESS(gpio_wakeup_enable(__hhal.usrbtn.pin, GPIO_INTR_LOW_LEVEL));

    /* button must be responsive, cannot wait for run loop */
    __hhal.usrbtn.task.hdl = xTaskCreateStatic(_btn_event_task,
                                               "_btn_event_task",
                                               HAL_TASK_STACK_SIZE,
                                               &__hhal, configMAX_PRIORITIES - 14,
                                               __hhal.usrbtn.task.stack,
                                               &__hhal.usrbtn.task.buffer);
    VERIFY_NOT_NULL(__hhal.usrbtn.task.hdl);

    return CAN5_SUCCESS;
}

static portTASK_FUNCTION(_btn_event_task, pvParameters)
{
    static const int64_t long_press_t = 5000;    // 5 sec
    static const int64_t press_seq_t  = 300;     // press sequence delay 300 ms

    struct hal_hdl_s* ___hhal = pvParameters;
    can5_btn_t *btn = &___hhal->usrbtn;
    uint32_t nval;
    int64_t block_t;

    block_t = portMAX_DELAY;

    while (true) {
        /* Block task to avoid cpu usage */
        if (xTaskNotifyWait(0, UINT32_MAX,
                            &nval, block_t) == pdFAIL) {
            can5_hal_evt_usrbtn_data_t evt_data;

            // timeout
            block_t = portMAX_DELAY;

            switch (btn->status) {

                case CAN5_BTN_STAT_INITD:
                    // nothing
                    break;
                case CAN5_BTN_STAT_PRESS:
                    evt_data.count = btn->press_count;
                    esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_USRBTN_PRESS, &evt_data, sizeof(evt_data), 100);
                    ESP_LOGI(TAG, "Pressed %d times", btn->press_count);
                    break;
                case CAN5_BTN_STAT_LONG_PRESS:
                    // already posted
                    break;
            }

            btn->status = CAN5_BTN_STAT_INITD;
            btn->press_count = 0;
            continue;
        }

        block_t = pdMS_TO_TICKS(press_seq_t);

        btn->last_press = esp_timer_get_time();

        while (gpio_get_level(btn->pin) == 0) {
            int64_t now = esp_timer_get_time();

            // esp_timer_get_time is in usec
            if (btn->status != CAN5_BTN_STAT_LONG_PRESS &&
                btn->last_press + long_press_t * 1000 < now) {
                btn->status = CAN5_BTN_STAT_LONG_PRESS;
                ESP_LOGI(TAG, "Long press");
                esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_USRBTN_LONG_PRESS, NULL, 0, 100);
            }

            vTaskDelay(1);
        }

        if (btn->status != CAN5_BTN_STAT_LONG_PRESS) {
            btn->status = CAN5_BTN_STAT_PRESS;
            btn->press_count++;
        }
        gpio_intr_enable(btn->pin);
    }
}

static void _hal_btn_handler(int lev)
//static void IRAM_ATTR _hal_btn_handler(int lev)
{
    // pull up button, low means pressed.
    if (lev == 0) {
        /* long presses causes interrupt to crash */
        gpio_intr_disable(__hhal.usrbtn.pin);
        xTaskNotify(__hhal.usrbtn.task.hdl,  0, eNoAction);
    }
}

//static void IRAM_ATTR _hal_isr_handler(void* arg)
static void _hal_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    int lev = gpio_get_level(gpio_num);
    char s[30];
    can5_hal_evt_t event = CAN5_HAL_EVT_NONE;
    snprintf(s, 30, "gpio: %d, level: %d", gpio_num, lev);
    switch (gpio_num) {
        case CAN5_PIN_WAKE:
            event = lev ? CAN5_HAL_EVT_WAKE_INT_RISE : CAN5_HAL_EVT_WAKE_INT_FALL;
            break;
        
        case CAN5_PIN_USRBTN:
            // button handler
            _hal_btn_handler(lev);
            return;

        case CAN5_PIN_RTCINT:
            event = lev ? CAN5_HAL_EVT_RTC_INT_RISE : CAN5_HAL_EVT_RTC_INT_FALL;
            break;

        default:
            break;
    }
    
    esp_event_post(CAN5_EVT_HAL, event, &s, sizeof(s), 100);
}

static void __wifi_connect_timer(void *args)
{
    /* issue #64. Do not reboot on WiFi client join failure.*/
    // int retires = *(int *)args;
    // if (retires > CONFIG_CAN5_HAL_WIFI_RETRIES_MAX) {
    //     can5_restart();
    // }
    esp_wifi_connect();
}

static uint64_t __wifi_get_next_connect()
{
    // double the time,in seconds
    uint64_t delay = __hhal.wifi.connect_retries * 2;
    // clamp to 60 seconds
    delay = delay > 60 ? 60 : delay;
    return delay * 1000000;
}

static void __wifi_event_sta_handler(int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Trying to connect to %s %s",
                 __hhal.wifi.sta.config.sta.ssid,
                 __hhal.wifi.sta.config.sta.password);

        esp_timer_start_once(__hhal.wifi.connect_timer, __wifi_get_next_connect());
        __hhal.wifi.status = CAN5_WIFI_STAT_CONNECTING;

    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "connect sta to %s : %s failed. reason %d. retry %d", event->ssid, event->bssid, event->reason, __hhal.wifi.connect_retries);
        esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_WIFI_STA_DISCONNECTED, NULL, 0, 100);
        CLEAR_ARRAY(__hhal.wifi.sta_ip);
        __hhal.wifi.connect_retries++;
        hal.sntp_stop();
        esp_timer_start_once(__hhal.wifi.connect_timer, __wifi_get_next_connect());
        __hhal.wifi.status = CAN5_WIFI_STAT_CONNECTING;

    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(__hhal.wifi.sta_ip, IP4ADDR_STRLEN_MAX, IPSTR, IP2STR(&event->ip_info.ip));

        __hhal.wifi.connect_retries = 0;
        //xEventGroupSetBits(__hhal.wifi.sta.event_group, NETIF_WIFI_EVT_GRP_GOT_IP);
        __hhal.wifi.status = CAN5_WIFI_STAT_CONNECTED;
        esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_WIFI_STA_CONNECTED, NULL, 0, 100);
        hal.sntp_start();
    }

}

static void __wifi_event_ap_handler(int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;

        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_WIFI_AP_CLIENT_JOINED, NULL, 0, 100);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;

        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_START) {
        esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_WIFI_AP_START, NULL, 0, 100);
    }
}

static void __wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "event %d", event_id);

    // station mode
    if (
        event_id == WIFI_EVENT_STA_START ||
        event_id == WIFI_EVENT_STA_DISCONNECTED ||
        event_id == IP_EVENT_STA_GOT_IP
        ) {
        __wifi_event_sta_handler(event_id, event_data);
    }

    // access point mode
    if (
        event_id == WIFI_EVENT_AP_STACONNECTED ||
        event_id == WIFI_EVENT_AP_STADISCONNECTED ||
        event_id == WIFI_EVENT_AP_START
        ) {
        __wifi_event_ap_handler(event_id, event_data);
    }
}

static void __cell_connect_timer(void *args)
{
    int retires = *(int *)args;

    if (retires > CONFIG_CAN5_HAL_CELL_RETRIES_MAX) {
        can5_restart();
    }

    can5_cmd_params_t run_cb = {
        .run_cb = {
            .run_cb = _cell_init,
            .run_cb_param = (void *)&_cell_init_params,
        }
    };

    can5_commander.add_cmd(CAN5_CMD_RUN_CB, &run_cb, NULL, NULL);
}

static uint64_t __cell_get_next_connect()
{
    // double the time,in seconds
    return __hhal.cell.connect_retries * 2 * 1000000;
}

static void _ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}


static void _ppp_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        //xEventGroupSetBits(event_group, CONNECT_BIT);
        snprintf(__hhal.cell.cell_ip, IP4ADDR_STRLEN_MAX, IPSTR, IP2STR(&event->ip_info.ip));
        esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_CELL_CONNECTED, NULL, 0, 100);
        hal.sntp_start();
        __hhal.cell.connect_retries = 0;
        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        CLEAR_ARRAY(__hhal.cell.cell_ip);
        esp_modem_set_mode(__hhal.cell.cell_dce, ESP_MODEM_MODE_COMMAND);
        esp_event_post(CAN5_EVT_HAL, CAN5_HAL_EVT_CELL_DISCONNECTED, NULL, 0, 100);
        hal.sntp_stop();
        __hhal.cell.connect_retries++;
        esp_timer_start_once(__hhal.cell.connect_timer, __cell_get_next_connect());
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}


//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0                                                        

/**
 * @brief Hal status tags
 * 
 */
static const can5_tag_tab_t _hal_stat_tags = {
    TAG_TAB_ITEM(HAL_STAT_UNINITD),
    TAG_TAB_ITEM(HAL_STAT_INITD)
};

/**
 * @brief Hal status tags
 * 
 */
static const can5_tag_tab_t _hal_evt_tags = {
    TAG_TAB_ITEM( CAN5_HAL_EVT_NONE        ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_INITIALIZED    ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_UPORT_RECVD ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_UPORT_RECVD ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_WAKE_INT_RISE   ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_WAKE_INT_FALL   ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_USRBTN_INT_RISE ),
    TAG_TAB_ITEM( CAN5_HAL_EVT_USRBTN_INT_FALL ),
    TAG_TAB_ITEM(CAN5_HAL_EVT_WIFI_STA_CONNECTED),
    TAG_TAB_ITEM(CAN5_HAL_EVT_WIFI_STA_DISCONNECTED),
    TAG_TAB_ITEM(CAN5_HAL_EVT_CELL_OPERATOR),
    TAG_TAB_ITEM(CAN5_HAL_EVT_CELL_CONNECTED),
    TAG_TAB_ITEM(CAN5_HAL_EVT_CELL_DISCONNECTED),
};

static const char* status_getstr(int32_t status) {
    return TAG_LOOKUP(status, _hal_stat_tags);
}

static const char* evt_getstr(int32_t evt) {
    return TAG_LOOKUP(evt, _hal_evt_tags);
}

#endif

