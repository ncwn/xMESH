/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can4_net_if.h
 * Descr: Describes the prototypes for the library implementing the function 
 *        to operate a communcation module compatible with the communication manager
 *        
 *        if_ops_c is the base interface to implement a library for a 
 *        communication module to be compatible with canarin 5 communcation manager
 *******************************************************************************/

#ifndef __CAN5_NET_IF__
#define __CAN5_NET_IF__

#include "can5_types.h"
#include "can5_wiring.h"
#include "can5_error.h"
#include <time.h>
#include <stdbool.h>
#include "limits.h"


#define CAN5_IFVER_LEN 7
#define CAN5_IFNAM_LEN 25
#define CAN5_IFMAN_LEN 25
#define CAN5_IFHWINFO_LEN 255


#define CAN5_RSSI_UNAVAILABLE INT16_MIN
#define CAN5_CREG_UNAVAILABLE 0xFFFFFFFF

// #define CAN5_IFSTATUS_STR_LEN (sizeof(comm_status_t)*5+1) // N status bits * 5char per bit + ending character

// Typedefs
//_____________________________________________________________________
// intercace status, single values meaning depends on the specific module
typedef int8_t   can5_net_status_t;
// application level communication port the meaning depends on the specific communication module 
typedef uint16_t can5_net_app_port_t;

// fuction prototypes
typedef void (can5_net_rxcb_f)(uint8_t netif_id, const void* rxdata, size_t len);

typedef enum can5_net_connect_evt_type_e {
    CAN5_NET_CONNEVT_NONE = 0x00,
    CAN5_NET_CONNEVT_CONNECTED,
    CAN5_NET_CONNEVT_SENDING,
    CAN5_NET_CONNEVT_SEND_COMPLETE,
    CAN5_NET_CONNEVT_SEND_FAILED,
    CAN5_NET_CONNEVT_DISCONNECTED,
    CAN5_NET_CONNEVT_DISCONNECT_TIMEOUT,
    CAN5_NET_CONNEVT_CONNECT_TIMEOUT,
    CAN5_NET_CONNEVT_CONNECT_FAILED,

    /*-----*/
    CAN5_NET_CONNEVT_LAST,
} can5_net_connect_evt_type_t;

typedef struct can5_net_connect_evt_s {
    can5_net_connect_evt_type_t type;
    uint8_t netif_id;
} can5_net_connect_evt_t;

// common netif status
#define CAN5_DRIVERCTL_NETIF_CONN_STATUS    0x99

// callback for connection events
typedef void (can5_net_conncb_f)(const can5_net_connect_evt_t* evt);

// Interface basic information Details including type, versio, name and manufacturer
typedef struct can5_netif_details_s {
    const can5_netif_type_t type;
    const char version[CAN5_IFVER_LEN+1];   // add one more byte for termination character
    const char name[CAN5_IFNAM_LEN+1];      // add one more byte for termination character
    const char manufacturer[CAN5_IFMAN_LEN+1];  // add one more byte for termination character
    char hwinfo[CAN5_IFHWINFO_LEN+1];  // add one more byte for termination character
} can5_netif_details_t;


typedef int16_t can5_netif_rssi_t;

/**
 * @brief Interface for network card drivers
 * 
 */
typedef struct can5_netif_ops_s {

    uint8_t (*get_id)();

    void (*set_id)(uint8_t id);

    // return 0 if detected, and save details in the specified location
    can5_err_t (*detect)(can5_port_idx_t port);  
    
    // send bytes through the interface 
    can5_err_t (*send)(const void* data, size_t len);

    // initialize the module to be ready to connect
    can5_err_t (*init)(can5_port_idx_t port);

    // initialize the module to be ready to connect
    can5_err_t (*uninit)();
    
    // rune once the card main function
    can5_err_t (*run)();

    /**
     * @brief Receive len bytes from the card and store data into prxdata buffer
     * @param prxdata pointer to return buffer
     * @param plen    pointer to size variable. It'll be used for number of byte to receive;
     * @param timeout  NOBOCK: if set to 0, do not block, return the bytes available in that moment
        *              MIN: 1000 / configTICK_RATE_HZ
        *              MAX: 0xFFFE
        *              BLOCK:If det to 0xFFFF Block indefinitely until the expected 
        *              number of bytes is received
     * 
     */
    can5_err_t (*recv)(void* prxdata, size_t* plen, uint16_t timeout);

    // set the callback function to execute upon reception of a packet on a specific port
    can5_err_t (*register_recv_cb)(can5_net_rxcb_f*);

    // return true if connection is considered established
    bool       (*is_connected)(void);

    /**
     * @brief Connect to the server
     * @param conncb Callback for connection events
     * @param wait   Time to wait for connection to be acquired
     *               If set to false return immediately. If specified,the callback will be called at connection or internal driver timeout event.
     *               If set to true return when the connection  is aquired or if the internal timeout expires. If specified, the  Callback is called fo all events.
     */
    can5_err_t (*connect)(can5_net_conncb_f* conncb, bool wait);

    /**
     * @brief run i/o commands
     * @param request The request integer
     * @param params  The params
     * @param response Response to this command
     */
    can5_err_t (*driverctl)(uint8_t request, void *params, void *response);

    // return rssi if available in dbm
    can5_netif_rssi_t (*rssi_get)(void);
    
    // Get driver status
    int32_t  (*status_get)();

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0 
    // print status in human readable format for debug
    const char*  (*status_getstr)(int32_t status);
#endif 

} can5_netif_ops_t;


// Structure used to store information of the currently used interface
// it includes the details, informations about the activity and a pointer to the operations functions set
typedef struct can5_netif_t {
    const can5_netif_details_t details;
    can5_netif_ops_t           ops;
    bool                       is_sleepable;    /* Can this interface sleep? */
} can5_netif_t;


#endif // CAN5_NET_IF
