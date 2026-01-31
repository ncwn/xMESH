/**************************************************
 * Author: rmukhia
 * Creation Date: 15/7/22
 * Description:  Miscellaneous features.
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_CMDR_H
#define CANARINV5_LEGO_CAN5_CMDR_H

#include "can5_module.h"
#include "can5_events.h"

union can5_cmd_params_u;

typedef can5_err_t (*can5_cmd_fn)(union can5_cmd_params_u *params); // command to execute
typedef void (*can5_cmd_cb)(can5_err_t ret, union can5_cmd_params_u *result_params,
                            void *user_data); // callback to execute with command status

typedef can5_err_t (*can5_cmdr_run_cb)(void *params);               // run callback function

typedef enum can5_cmd_type_e {
    CAN5_CMD_COMMIT_FS_DICTIONARY,      // commit filesystem dictionary
    CAN5_CMD_GET_FIRMWARE_VERSION,
    CAN5_CMD_RESET_AFTER,
    CAN5_CMD_ACTIVATE_NETWORK_LOGGERS,
    CAN5_CMD_SCAN_WIFI,
    CAN5_CMD_PING_IP,
    CAN5_CMD_ENABLE_WIFI_AP,
    CAN5_CMD_FACTORY_RESET,
    CAN5_CMD_CLEAR_ALL_SENSOR_DATA,
    CAN5_CMD_CLEAR_OLD_SENSOR_DATA,
    CAN5_CMD_POST_EVENT,
    CAN5_CMD_RECALC_JOBS,
    CAN5_CMD_RUN_CB,
} can5_cmd_type_t;

typedef struct can5_misc_s {
    can5_module_t module;
    can5_err_t (*add_cmd)(can5_cmd_type_t type, union can5_cmd_params_u *params,
                          can5_cmd_cb cb, void *user_data);
} can5_cmdr_t;


typedef struct can5_misc_cmd_wifi_scan_params_s {
    uint16_t max_scan;
    void *ap_info;
    uint16_t ap_count;
} can5_cmd_wifi_scan_params_t;

typedef struct can5_misc_cmd_ping_ip_params_s {
    char address[32];
    char *result;
} can5_cmd_ping_ip_params_t;

typedef struct can5_cmd_cmdr_evt_params_s {
    can5_cmdr_evt_t                    event;               // cmdr event
    time_t                             timeout_ms;          // timeout
} can5_cmd_cmdr_evt_params_t;

typedef struct  can5_cmd_cmdr_run_cb_s {
    can5_cmdr_run_cb run_cb;
    void *run_cb_param;
} can5_cmd_cmdr_run_cb_t;

typedef union can5_cmd_params_u {
    time_t                             restart_after;      // restart
    can5_cmd_wifi_scan_params_t        wifi_scan;          // wifi_scan
    can5_cmd_ping_ip_params_t          ping_ip;
    can5_cmd_cmdr_evt_params_t         cmdr_event;
    can5_cmd_cmdr_run_cb_t             run_cb;

} can5_cmd_params_t;


extern can5_cmdr_t can5_commander;

can5_err_t can5_commander_loop(void);

const char *can5_cmdr_evt_getstr(can5_cmdr_evt_t evt);


#endif //CANARINV5_LEGO_CAN5_CMDR_H
