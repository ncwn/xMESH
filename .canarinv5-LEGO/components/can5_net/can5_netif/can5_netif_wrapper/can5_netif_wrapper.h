/**
 * @file   can5_netif_template.h
 * @author Luca De Mori (luca.demori.it@gmail.com)
 * @brief 
 * @version 0.1
 * @date    2021-02-10
 * 
 * 
 */

#ifndef __CAN5_NETIF_WPPR_H__
#define __CAN5_NETIF_WPPR_H__

#include "can5_netif.h"

typedef enum can5_netif_wppr_driverctl_e {
    CAN5_DRIVERCTL_NETIF_WPPR_NONE,
    CAN5_DRIVERCTL_NETIF_WPPR_CONN_STATUS = CAN5_DRIVERCTL_NETIF_CONN_STATUS,
} can5_netif_wppr_driverctl_t;

typedef struct can5_netif_wppr_driverctl_params_s {
    uint16_t scan_list_size;
} can5_netif_wppr_driverctl_params_t;

#define NETIF_WPPR_MAX_RECV_SIZE        1024
//_______________________________________________________________________________________________________
//
//   TYPES DECLARATION
//-------------------------------------------------------------------------------------------------------


//_______________________________________________________________________________________________________
//
//   GLOBAL VARIABLES
//-------------------------------------------------------------------------------------------------------

extern const can5_netif_t netif_wppr;


//_______________________________________________________________________________________________________
//
//   FUNCTIONS DECLARATION
//-------------------------------------------------------------------------------------------------------


//_______________________________________________________________________________________________________
//
//   DEBUG SUPPORT
//-------------------------------------------------------------------------------------------------------


#endif // __CAN5_NETIF_WPPR_H__
