/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can5_wiring.h
 * Descr: Wiring specification for Canarin 5 platform periferials 
 *******************************************************************************/

#ifndef __CAN5_WIRING_H__
#define __CAN5_WIRING_H__

#include <stdint.h>
#include "can5_pins.h"

// PERIPHERIALS ALLOCATION
// _____________________________________________________________

#define CAN5_UART_NETPORT  UART_NUM_1
#define CAN5_UART_UPORT    UART_NUM_2
#define CAN5_I2C_NUM       I2C_NUM_0
//#define CAN5_I2C_SPEED     400000
/* ATTiny3226 needs to be at 20MHz to run i2c fast mode. */
#define CAN5_I2C_SPEED     100000

#define CAN5_ADC_UNIT      ADC_UNIT_1 // Pin 33
#define CAN5_ADC_CHANNEL   ADC_CHANNEL_5 // Pin 33


#define CAN5_ADPORT_MUX_SEL_BITS  2 //CONFIG_CAN5_ADPORT_MUX_SEL_BITS
#define CAN5_UPORT_MUX_SEL_BITS   3 //CONFIG_CAN5_UPORT_MUX_SEL_BITS
#define CAN5_MUX_SEL_BITS_MAX     3

/**
 * @brief Type for pins
 * 
 */
typedef int can5_pin_t;

typedef struct can5_muxd_port_wiring_s {
    const can5_pin_t inhib;
    const can5_pin_t selct[CAN5_MUX_SEL_BITS_MAX];
    const uint8_t    selct_size;   /**< number of bits used for select */
} can5_muxd_port_wiring_t;


// // PCI Express Mini Card pinout
// // TODO: finish allocation
// typedef struct pcie_mini_pinmap_s {
// /*01*/ uint8_t wake_;      // activa low
// /*03*/ uint8_t coex1;
// /*05*/ uint8_t coex2;
// /*07*/ uint8_t clkreq_;   // active low
// /*08*/ //uint8_t uim_pwr;
// /*10*/ //uint8_t uim_data;
// /*11*/ uint8_t refclk_n;     // maybe connect to UART TX
// /*12*/ //uint8_t uim_clk;
// /*13*/ uint8_t refclk_p;     // maybe connect to UART RX
// /*14*/ //uint8_t uim_reset;
// /*16*/ //uint8_t uim_vpp;
// /*17*/ uint8_t reserved_17;
// /*19*/ uint8_t reserved_19;
// /*20*/ uint8_t w_disable_;  // active low
// /*22*/ uint8_t perst_;      // active low
// /*23*/ //uint8_t pern0;   // maybe connect to UART CTS
// /*25*/ //uint8_t perp0;   // maybe connect to UART RTS
// /*30*/ uint8_t smb_clk;
// /*31*/ //uint8_t petn0;    // maybe connect to UART DTR
// /*32*/ uint8_t smb_data;
// /*33*/ //uint8_t petp0;    // maybe connect to UART DCD
// /*36*/ uint8_t usb_dn;
// /*38*/ uint8_t usb_dp;
// /*42*/ //uint8_t led_wwan_;  // active low
// /*44*/ //uint8_t led_wlan_;  // active low
// /*45*/ uint8_t reserved45; // spi SCK
// /*46*/ //uint8_t led_wpan_;  // active low
// /*47*/ uint8_t reserved47; // spi MISO
// /*49*/ uint8_t reserved49; // spi MOSI
// /*51*/ uint8_t reserved51; // spi CSN
// } pcie_mini_pinmap_t;

// pci connectors array
// extern pcie_mini_pinmap_t pcie_ports[CAN5_PCIE_NUM];

/*
* DIO pins
* The DIO (digitial I/O) pins on the transceiver board can be configured for various functions. The LMIC library uses them to get instant status information from the transceiver. For example, when a LoRa transmission starts, the DIO0 pin is configured as a TxDone output. When the transmission is complete, the DIO0 pin is made high by the transceiver, which can be detected by the LMIC library.
*
* The LMIC library needs only access to DIO0, DIO1 and DIO2, the other DIOx pins can be left disconnected. On the Arduino side, they can connect to any I/O pin, since the current implementation does not use interrupts or other special hardware features (though this might be added in the feature, see also the "Timing" section).
*
* In LoRa mode the DIO pins are used as follows:
*
* DIO0: TxDone and RxDone
* DIO1: RxTimeout
* In FSK mode they are used as follows::
*
* DIO0: PayloadReady and PacketSent
* DIO2: TimeOut
*/



// PMS
// _____________________________________________________________
// typedef struct pms_wiring_s {
//     uint8_t enable;
//     HardwareSerial& serial;
// } pms_wiring_t;
// extern pms_wiring_t pms_wiring;

#endif //__CAN5_WIRING_H__