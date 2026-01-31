/**************************************************
 * Author: rmukhia
 * Creation Date: 15/11/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_MQTT_H
#define CANARINV5_LEGO_CAN5_MQTT_H
#include "can5_module.h"
#include "can5_events.h"

typedef struct can5_mqtt_data_rx_s {
    int msg_id;
    char *data;
    int data_len;
} can5_mqtt_data_rx_t;

can5_err_t can5_mqtt_client_init();
can5_err_t can5_mqtt_client_uninit();

int can5_mqtt_publish(const char **name_components, size_t name_components_len,
                      const char *data, size_t len, int qos, int retain);

const char *can5_mqttclient_evt_getstr(can5_mqttclient_evt_t evt);


#endif //CANARINV5_LEGO_CAN5_MQTT_H
