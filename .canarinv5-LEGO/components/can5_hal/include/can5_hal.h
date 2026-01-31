/*******************************************************************************
 * Author: Luca De Mori @liuked
 * Date:   27-03-2020
 * 
 * File:  can5_hal.h
 * Descr: Serving function 
 *******************************************************************************/

#ifndef __CAN5_HAL_H__
#define __CAN5_HAL_H__

#include <stdint.h>
#include <stdbool.h>
#include "can5_wiring.h"
#include "can5_error.h"
#include "can5_module.h"
#include "can5_types.h"
#include "esp_event_base.h"
#include "hal/i2c_types.h"
#include "esp_wifi_types.h"



#define CAN5_SERIAL_BLOCKING 0xFFFF

//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------


typedef struct can5_hal_evt_usrbtn_data_s {
    int count;                              // times pressed
} can5_hal_evt_usrbtn_data_t;

typedef struct can5_serial_pattern_event_data_s {
    can5_port_idx_t port;
    int             pattern_pos;
} can5_serial_pattern_event_data_t;

typedef void (*can5_serial_pattern_func)(const can5_serial_pattern_event_data_t *pattern_event_data);

/**
 * @brief Module available functions
 * 
 */
typedef struct can5_hal_s {
    /**
     * @brief Module standard fnuctions
     * 
     */
    can5_module_t module;


    /**
     * @brief  Enable or disable the port
     * @todo   add semaphores if using multi thread
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY          resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT  port out of range
     */
    can5_err_t (*enable)(bool enable, can5_port_idx_t port);

    /**
     * @brief  Return port enable status
     * 
     * @return true: enabled
     * @return false; disabled
     */
    bool (*is_enabled)(can5_port_idx_t port);

    /**
     * @brief Send a buffer pdata over UART at port port
     * @todo add semaphores if using multi thread
     * @details Accepts only ports between UPORT_0 and UPORT_COUNT-1
     * 
     * @param pdata pointer to static data buffer
     * @param len   number of bytes to tx
     * @param port  tx port 
     * @param to_ms timeout in milliseconds to wait for tx to be completed. 
     *              0 to return immediately
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY          resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT  port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     */
    can5_err_t (*serial_send)(const void* pdata, size_t len, can5_port_idx_t port,  uint16_t to_ms);

    /**
     * @brief Receive from UART at port, into buffer pbuffer of size pointed by plen
     * @details Accepts only ports between UPORT_0 and UPORT_COUNT-1 
     *          plen is set to the size of the received payload. In case of error plen is set to 0.
     * @todo add semaphores if using multi thread
     * 
     * @param pdata pointer to data buffer for rx
     * @param len   number of bytes to receive (if less -> timeout)
     *              The function populates this variable with the number of bytes received
     * @param port  rx port 
     * @param to_ms NOBOCK: if set to 0, do not block, return the bytes available in that moment
     *              MIN: 1000 / configTICK_RATE_HZ
     *              MAX: 0xFFFE
     *              BLOCK:If det to 0xFFFF Block indefinitely until the expected 
     *              number of bytes is received
     * 
     * @return CAN5_SUCCESS            
     * @return CAN5_HAL_ERR_BUSY          resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT  port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     * @return CAN5_HAL_ERR_NO_PATTERN    pattern hasbeen installed for this port and no pattern has been detected yet
     * @return CAN5_HAL_ERR_SERIAL_RECV   error in the ESP serial receive function
     * @return CAN5_HAL_ERR_TIMEOUT       receive timeout
     */
    can5_err_t (*serial_recv)(void* pbuffer, size_t* plen, can5_port_idx_t port, uint16_t to_ms);

    /**
     * @brief  Install uart task
     *
     * @param port to install on
     * @param cb To call on pattern detect
     * @return CAN5_SUCCESS            
     * @return CAN5_HAL_ERR_BUSY          resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT  port out of range
     */
    can5_err_t (*serial_char_detect_install)(char ch, can5_port_idx_t port, can5_serial_pattern_func cb);
   
    /**
     * @brief 
     * 
     * @return CAN5_SUCCESS            
     * @return CAN5_HAL_ERR_BUSY          resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT  port out of range
     */
    can5_err_t (*serial_char_detect_remove)(can5_port_idx_t port);

    /**
     * @brief Write buffer pdata on I2C (check if port has been enabled first)
     * @todo add semaphores if using multi thread
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     * 
     */
    can5_err_t (*i2c_write_reg)(uint8_t addr7, uint8_t reg, uint8_t* data, size_t len, bool ack_check);

    /**
     * @brief Read from I2C into pbuffer (check if port has been enabled first)
     * @todo add semaphores if using multi thread
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     * 
     */
    can5_err_t (*i2c_read_reg)(uint8_t addr7, uint8_t reg, uint8_t* dataout, size_t len, i2c_ack_type_t ack_type);

    /**
     * @brief Read the signal at the specified port
     * @todo add semaphores if using multi thread
     * @details Accepts only ports between ADPORT_0 and ADPORT_COUNT-1
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     */
    bool       (*digital_read)  (can5_port_idx_t port);

    /**
     * @brief Write 0 or 1 at the specified port
     * @todo add semaphores if using multi thread
     * @details Accepts only ports between ADPORT_0 and ADPORT_COUNT-1
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     */
    can5_err_t (*digital_write) (bool value, can5_port_idx_t port);
    
    /**
     * @brief Read analog value at the specified port
     * @details set the port to analog and read
     * 
     * @todo add semaphores if using multi thread
     * @details Accepts only ports between ADPORT_0 and ADPORT_COUNT-1
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     */
    can5_err_t (*analog_read)   (uint16_t* const psample, can5_port_idx_t port);
    
    /**
     * @brief Write 0 or 1 at the specified port
     * @todo add semaphores if using multi thread
     * @details Accepts only ports between ADPORT_0 and ADPORT_COUNT-1
     * 
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     */
    can5_err_t (*analog_write)  (uint16_t pwm, can5_port_idx_t port);

    /**
     * @brief Reset NETPORT_1 device.
     * @todo add semaphores if using multi thread
     * @details Reset netport 1 device (lwan, cell)
     *
     * @return CAN5_SUCCESS
     * @return CAN5_HAL_ERR_BUSY         resources is busy and cannot be acquired
     * @return CAN5_HAL_ERR_PORT_DISABLED port disabled
     * @return CAN5_HAL_ERR_INVALID_PORT port out of range
     */
    can5_err_t (*reset_netport1)  ();

    /**
     * @brief Start Wifi is mode.
     * @param mode Device mode.
     * @return CAN5_SUCCESS
     */
    can5_err_t (*wifi_start) (bool start_ap, bool start_sta, const char *sta_ssid, const char *sta_pass,
                                 const char *ap_ssid, const char *ap_pass, bool enable_sntp, const char *sntp_server);

    /**
     * @brief Start Cell is mode.
     * @param enable_net
     * @param enable_gps
     * @param apn
     * @param enable_sntp
     * @param sntp_server
     * @return CAN5_SUCCESS
     */
    can5_err_t (*cell_start)(bool enable_net, bool enable_gps, const char *apn, bool enable_sntp, const char *sntp_server);

    int16_t (*get_wifi_rssi)();
    int16_t (*get_cell_rssi)();

    can5_err_t (*get_cell_gps)(char *out_str);

    /**
     * @brief Get STA ip address
     * @return Ip address in string
     */
    const char *(*get_ip_sta)();

    const char *(*get_ip_cell)();

    can5_err_t (*get_bms_battery_level)(float *val);

    can5_err_t (*sntp_start)();

    can5_err_t (*sntp_stop)();

    can5_err_t (*shutdown_cb)();

    can5_err_t (*get_mac_addresses)(uint8_t *ap_mac, uint8_t *sta_mac);

    can5_err_t (*get_device_id)(uint64_t *device_id);

    can5_err_t (*print_cell_status)();

    can5_err_t (*get_wifi_sta_status)(wifi_ap_record_t *record, bool *connected);

#ifdef CONFIG_IDF_TARGET_ESP32S3
    can5_err_t (*enable_sens_switch)(bool enable);

    can5_err_t (*enable_net_switch)(bool enable);

    can5_err_t (*pm_enable)(bool enable);

    can5_err_t (*set_status_led)(bool enable);
#endif

} can5_hal_t;



//_______________________________________________________________________________________________________
//
//   GLOBAL VARIABLES
//-------------------------------------------------------------------------------------------------------

extern can5_hal_t hal;


//_______________________________________________________________________________________________________
//
//   FUNCTIONS DECLARATION
//-------------------------------------------------------------------------------------------------------


//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0                                                        

const char* can5_hal_port_getstr(int32_t evt);  
#else 

#define can5_hal_port_getstr(x) ""

#endif


#endif // __CAN5_HAL_H__
