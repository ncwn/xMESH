/*******************************************************************************
* Author: @rmukhia
* Date:   7/11/22
*
* File:  can5_sensor_ws3226.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_WS3226_H
#define CAN5_APP_CAN5_SENSOR_Ws3226_H

#include "can5_sensordriv.h"

#define WS3226_POLL_INTERVAL                       (60 * 1000)  // 60 seconds between each reading

typedef union ws3226_ctl_params_s {
    struct {
        int start_time_s;
        int period_s;
    } data_cycle_params;
} ws3226_ctl_params_t;

typedef enum can5_sensor_ws3226_ctl_e {
    SENSOR_WS3226_RESET = 0x0,
} can5_sensor_ws3226_ctl_t;

typedef struct can5_sensor_ws3226_data_s {
    float rain;
    float wind_spd;
    float wind_dir;
    float battery_voltage;
    uint8_t current_idx;
    time_t timestamp;
    TAILQ_ENTRY(can5_sensor_ws3226_data_s) te;
} can5_sensor_ws3226_data_t;

typedef TAILQ_HEAD(can5_sensor_ws3226_data_list_head_s, can5_sensor_ws3226_data_s) can5_sensor_ws3226_data_list_head_t;


extern const can5_sensordriv_t sensordriv_ws3226;

void parse_ws3226_read_data(const can5_sensordriv_t *driv, const can5_sensor_ws3226_data_list_head_t *ws3226_data_list,
                            void *pout, size_t *plen);

// get 10 bytes serial number
void get_ws3226_id(const can5_sensor_hdl_t *hdl, char **serial_num);

#endif //CAN5_APP_CAN5_SENSOR_WS3226_H