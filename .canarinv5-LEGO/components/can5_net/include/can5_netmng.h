/*******************************************************************************
 * Author: Luca De Mori @lucadm94
 * Date:   27-03-2020
 * 
 * File:  can4_momm_manager.h
 * Descr: Comunication Manager Module for Canarin5
 *        It allows the automatic detection of network interfaces connected to 
 *        the pcie ports and abstract the nic operations using generc functions 
 *        exposed to other modules and to the main routine
 *******************************************************************************/

#ifndef __CAN5_NETMNG_H__
#define __CAN5_NETMNG_H__

#include "can5_netif.h"
#include "can5_module.h"
#include "can5_netproto.h"

// Defines
//_____________________________________________________________________


// Types
//_____________________________________________________________________

typedef struct can5_netmng_connected_protos_s {
    const char *proto_name;
    const char *status_str;
    bool is_connected;
} can5_netmng_connected_protos_t;

/**
 * @brief Network events
 * 
 */


/**
 * @brief Module definition
 * 
 */
typedef struct can5_netmng_s {
    can5_module_t module;

    /**
     * @brief Return if the active nic is connected
     * @return  length of is connected structure
     */
    size_t (*is_connected)(can5_netmng_connected_protos_t *connected_protos);
} can5_netmng_t;

// Variables declaration
//_____________________________________________________________________

extern const can5_netmng_t net;

#if defined(CONFIG_LOG_DEFAULT_LEVEL) && CONFIG_LOG_DEFAULT_LEVEL>0
/**
 * @brief Return a string corresponding to the connection event type
 *
 * @return const char* event description
 */
const char * connevt_getstr(can5_net_connect_evt_type_t type);
#endif



//         static bool load_selected_interface(void* const dest, uint16_t size, void* const pdata);
//         static int8_t comm_interface_tostring(char* strgdest, uint16_t strglen, const void* data, uint16_t datalen, void* const pdata);
        
//         size_t detect_if();

//         // initiate connection to the network
//         can5_err_t connect();
        
//         // disconnect from the network
//         can5_err_t disconnect();

//         // disconnect and reconnect (reset network parameters)
//         can5_err_t reconnect();

//         // sent bytes through the network
//         size_t send(const void* bytes, size_t len);

//         // put the radios to sleep
//         can5_err_t sleep(time_t ms);

//         // set the callback function on reception of packets on a specific port
//         can5_err_t register_recv_cb(const can5_net_rxcb_f*);

//         // print in readable format the stats (for debug purposes)
//         void print_status();

//         // return the port number with the selected if based on the configured if type 
//         can5_net_phy_port_t select_if();

//         // link the modules libraries
//         can5_netif_ops_c* if_ops[CAN5_NET_SUPPORTED_IF];

//         // array  holding the details of the detected interfaces 
//         // including the type, used to retrive the right functions to operate it
//         // The index of the array correspont to the index of the pcie connectors
//         if_details_t detected_if[CAN5_NET_MAX_IFS];

//         // structure holdinf all info about the interface in use
//         // TODO, make the details field a pointer to the detected_if array (save space)
//         can5_netif_t iface;
        
//         can5_net_phy_port_t _selected_if; // indicateds the comm port where the selected adapter is connected
//         // if_type_t _selected_interface_type;
//         can5_net_phy_port_t _available_ifs[CAN5_NET_MAX_IFS];
//         can5_net_phy_port_t PROGMEM _def_selected_if = CAN5_DEFAULT_NET_IF;

// // available global functions
// //_____________________________________________________________________
// // print interface details for debug purposes
// void print_if_details(if_details_t* dt);

// // Variables
// //_____________________________________________________________________
// // global variable for accessing the module
// extern NetworkManager netmng;
        

#endif // CAN5_netmng_H