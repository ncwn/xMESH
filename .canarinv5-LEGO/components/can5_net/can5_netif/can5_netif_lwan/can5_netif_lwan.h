/**
 * @file   can5_netif_template.h
 * @author Luca De Mori (luca.demori.it@gmail.com)
 * @brief 
 * @version 0.1
 * @date    2021-02-10
 * 
 * 
 */

#ifndef __CAN5_NETIF_TEMPLATE_H__
#define __CAN5_NETIF_TEMPLATE_H__

#include "can5_netif.h"

//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------

#define LWAN_SEND_DELAY_DEFAULT  8000

#define NETIF_LWAN_MAX_RECV_SIZE 256

typedef enum can5_netif_lwan_driverctl_e {
    CAN5_DRIVERCTL_NETIF_LWAN_GET_SEND_DELAY,
    CAN5_DRIVERCTL_NETIF_LWAN_RESET_FRAME_COUNTERS,
    CAN5_DRIVERCTL_NETIF_LWAN_CONN_STATUS = CAN5_DRIVERCTL_NETIF_CONN_STATUS,
} can5_netif_lwan_driverctl_t;


//_______________________________________________________________________________________________________
//
//   GLOBAL VARIABLES
//-------------------------------------------------------------------------------------------------------


extern const can5_netif_t netif_lwan;


//_______________________________________________________________________________________________________
//
//   FUNCTIONS DECLARATION
//-------------------------------------------------------------------------------------------------------


//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#endif // __CAN5_NETIF_TEMPLATE_H__