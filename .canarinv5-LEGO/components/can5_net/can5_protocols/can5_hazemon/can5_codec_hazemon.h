/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   22/02/22
*
* File:  can5_codec_udp.h
* Descr:
*******************************************************************************/

#include "can5_sensor_data.h"
#include "can5_hazemon_types.h"

#ifndef CAN5_APP_CAN5_CODEC_UDP_H
#define CAN5_APP_CAN5_CODEC_UDP_H

/* the commands we can receive from the server */
typedef struct hazemon_rx_cmd_s {
    hazemon_type_t type;
    // for now its val
    char val[32];
    TAILQ_ENTRY(hazemon_rx_cmd_s) te;
} hazemon_rx_cmd_t;

typedef TAILQ_HEAD(hazemon_rx_cmd_list_s, hazemon_rx_cmd_s) hazemon_rx_cmd_list_t;

can5_err_t hazemon_make_tx_packet(const can5_sensor_data_list_t *list, uint8_t *out_pkt, size_t *out_len);
// list should be an initialized list head and not a dangling pointer.
can5_err_t hazemon_parse_rx_packet(const uint8_t *data, size_t buf_len, hazemon_rx_cmd_list_t *out_list);

void hazemon_rx_cmd_list_free(hazemon_rx_cmd_list_t *list, bool free_list);

#endif //CAN5_APP_CAN5_CODEC_UDP_H
