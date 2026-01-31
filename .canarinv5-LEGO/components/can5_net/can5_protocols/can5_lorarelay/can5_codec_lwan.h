/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   14/02/22
*
* File:  can5_codec_lwan.h
* Descr:
*******************************************************************************/

#ifndef TEST_APP_CAN5_CODEC_LWAN_H
#define TEST_APP_CAN5_CODEC_LWAN_H

#include <can5_sensor_data.h>
#include "can5_loramsg.h"

#define LWAN_MAX_DATA_PER_CYCLE     32
#define LWAN_MAX_PACKETS_PER_CYCLE  32

typedef enum lwan_rx_pkt_cmd_type_e {
    LWAN_RX_CMD_DATA = 0,
    LWAN_RX_CMD_REBOOT,
    LWAN_RX_CMD_INTERVAL,
    LWAN_RX_CMD_RESET_FRAME_CTR,
    LWAN_RX_CMD_REJOIN,
} lwan_rx_pkt_cmd_type_t;

typedef struct lwan_rx_pkt_t {
    lwan_rx_pkt_cmd_type_t type;
    union {
        uint32_t args;
        struct {
            uint8_t cycle_id;
            uint8_t seq_id[LWAN_MAX_DATA_PER_CYCLE];
            size_t num_seq;
        } data;
    };
} lwan_rx_pkt_t;

typedef struct lwan_tx_pkt_s {
    uint8_t cycle_id;
    uint8_t seq_id[LWAN_MAX_DATA_PER_CYCLE];
    size_t num_seq;
    bool ack_received[LWAN_MAX_DATA_PER_CYCLE];
    struct can5_lmsg_packet *pkt;
    size_t len;
} lwan_tx_pkt_t;

typedef struct lwan_tx_pkt_list_s {
    lwan_tx_pkt_t pkt[LWAN_MAX_PACKETS_PER_CYCLE];
    size_t count;
} lwan_tx_pkt_list_t;

typedef struct lwan_make_tx_packets_args_s {
    int datarate;
    int user_max_bytes;                 // use user defined max bytes, if the is not zero then this will take effect
    uint8_t cycle_ctr;
    bool skip_gps;
    size_t num_cycle_data;
    bool add_num_cycle_data;
} lwan_make_tx_packets_args_t;

can5_err_t lwan_make_tx_packets(const can5_sensor_data_list_t *list, lwan_tx_pkt_list_t *out_pkt_list,
                                const lwan_make_tx_packets_args_t *args);
can5_err_t lwan_parse_rx_packet(const uint8_t *data, size_t buf_len, lwan_rx_pkt_t *res);

const char *loramsg_sensor_type_str(int type);
#endif //TEST_APP_CAN5_CODEC_LWAN_H
