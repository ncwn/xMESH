/**************************************************
 * Author: rmukhia
 * Creation Date: 30/9/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_ACTUATORDRIV_H
#define CANARINV5_LEGO_CAN5_ACTUATORDRIV_H

#include <stddef.h>
#include "can5_error.h"
#include "can5_types.h"
#include "can5_phy_io.h"
#include "can5_actuator_cmd.h"

typedef struct can5_actuator_hdl_s {
    void *hdl;                                  // hdl pointer
    char hdl_data[];                            // hdl data
} can5_actuator_hdl_t;

typedef struct can5_actuatordriv_ops_s {

    can5_actuator_hdl_t *(*alloc)(size_t *len);

    uint8_t (*get_id)(can5_actuator_hdl_t *hdl);

    void (*set_id)(can5_actuator_hdl_t *hdl, uint8_t id);

    // return 0 if detected, and save details in the specified location
    can5_err_t (*detect)(can5_actuator_hdl_t *hdl, can5_port_idx_t port);

    // initialize the module to be ready to read
    can5_err_t (*init)(can5_actuator_hdl_t *hdl, can5_port_idx_t port);

    // deinitialize the module
    can5_err_t (*uninit)(can5_actuator_hdl_t *hdl);

    can5_err_t (*command)(can5_actuator_hdl_t *hdl,
        can5_actuator_cmd_t cmd, const can5_actuator_cmd_params_t *params);

} can5_actuatordriv_ops_t;

#define CAN5_ACTUATOR_ID_NONE  0xFF

typedef struct can5_actuatordriv_s {
    can5_actuatordriv_ops_t ops;
    can5_phy_io_details_t details;
} can5_actuatordriv_t;


#endif //CANARINV5_LEGO_CAN5_ACTUATORDRIV_H
