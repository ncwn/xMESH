/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can5_wiring.h
 * Descr: Wiring specification for Canarin 5 platform periferials 
 *******************************************************************************/

#ifndef __CAN5_PINS_H__
#define __CAN5_PINS_H__

#include <stdint.h>

#define CAN5_FATFS_MOUNT_DIR "/sdcard"

// INPUT BUTTONS
#define CAN5_PIN_BOOT_BUTTON 0

// PIN MAPPING
// _____________________________________________________________
#define CAN5_PIN_UNUSED 0xFF

#if CONFIG_IDF_TARGET_ESP32

#define CAN5_PIN_PORT_MUX_INH  12  /**< inhibit Input for safe selection */

#define CAN5_PIN_WAKE          39

#define CAN5_PIN_USRBTN        0
#define CAN5_PIN_RTCINT        34
#define CAN5_PIN_BSMINT        CAN5_PIN_UNUSED

#define CAN5_PIN_NETPORT_TX    32
#define CAN5_PIN_NETPORT_RX    35
#define CAN5_PIN_NETPORT_PERST 13
//#define CAN5_PIN_NETPORT_TX    16
//#define CAN5_PIN_NETPORT_RX    17
//#define CAN5_PIN_NETPORT_PERST 33

#define CAN5_PIN_ADPORT_EN     14
#define CAN5_PIN_ADPORT_SIG    33
#define CAN5_PIN_ADPORT_SEL0   25
#define CAN5_PIN_ADPORT_SEL1   26
  
#define CAN5_PIN_UPORT_EN      27
#define CAN5_PIN_UPORT_TX      16
#define CAN5_PIN_UPORT_RX      17
//#define CAN5_PIN_UPORT_TX      32
//#define CAN5_PIN_UPORT_RX      35
#define CAN5_PIN_UPORT_SEL0    4
#define CAN5_PIN_UPORT_SEL1    2
#define CAN5_PIN_UPORT_SEL2    15

#define CAN5_PIN_SDA           21
#define CAN5_PIN_SCL           22

#define CAN5_PIN_SDSPI_MOSI    23
#define CAN5_PIN_SDSPI_MISO    19
#define CAN5_PIN_SDSPI_CLK     18
#define CAN5_PIN_SDSPI_CS      5

#elif CONFIG_IDF_TARGET_ESP32S3

#define CAN5_PIN_PORT_MUX_INH  7  /**< inhibit Input for safe selection */

#define CAN5_PIN_WAKE          CAN5_PIN_UNUSED

#define CAN5_PIN_USRBTN        0
#define CAN5_PIN_RTCINT        6
#define CAN5_PIN_BSMINT        CAN5_PIN_UNUSED

#define CAN5_PIN_NETPORT_TX    17
#define CAN5_PIN_NETPORT_RX    18
#define CAN5_PIN_NETPORT_PERST 38

#define CAN5_PIN_UPORT_TX      48
#define CAN5_PIN_UPORT_RX      47
#define CAN5_PIN_UPORT_SEL0    15
#define CAN5_PIN_UPORT_SEL1    16

#define CAN5_PIN_UPORT_SEL2    CAN5_PIN_UNUSED


#define CAN5_PIN_SDA           1
#define CAN5_PIN_SCL           2

#define CAN5_PIN_SDSPI_MOSI    11
#define CAN5_PIN_SDSPI_MISO    13
#define CAN5_PIN_SDSPI_CLK     12
#define CAN5_PIN_SDSPI_CS      10

#define CAN6_PIN_PM_EN         21

#define CAN6_PIN_NET_PWR_EN         4
#define CAN6_PIN_SEN_PWR_EN         5

#define CAN6_PIN_LED_STATUS         8

#define CAN6_PIN_RGB_GPIO           14


#endif


#endif //__CAN5_PINS_H__