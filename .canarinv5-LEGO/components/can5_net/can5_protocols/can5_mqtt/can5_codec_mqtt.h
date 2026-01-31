/**************************************************
 * Author: rmukhia
 * Creation Date: 17/11/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_CODEC_MQTT_H
#define CANARINV5_LEGO_CAN5_CODEC_MQTT_H

#include "can5_sensor_data.h"

// out_list should be an initialized list head and not a dangling pointer.
can5_err_t mqtt_make_tx(const can5_sensor_data_list_t *list, char **out_str, char *out_timestamp);

#endif //CANARINV5_LEGO_CAN5_CODEC_MQTT_H
