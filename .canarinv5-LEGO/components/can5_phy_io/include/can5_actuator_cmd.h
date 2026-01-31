/**************************************************
 * Author: rmukhia
 * Creation Date: 3/10/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_ACTUATOR_CMD_H
#define CANARINV5_LEGO_CAN5_ACTUATOR_CMD_H

#include <time.h>

typedef enum can5_actuator_cmd_e {
    CAN5_ACTUATOR_CMD_RELAY_LOW = 0,
    CAN5_ACTUATOR_CMD_RELAY_HIGH,

    CAN5_ACTUATOR_CMD_COUNT,
    CAN5_ACTUATOR_CMD_NONE = 0xff
} can5_actuator_cmd_t;

typedef struct can5_actuator_cmd_params_s {
    can5_port_idx_t port;
    union {
        struct {
            time_t time_ms;
        } relay;
    };
} can5_actuator_cmd_params_t;

#endif //CANARINV5_LEGO_CAN5_ACTUATOR_CMD_H
