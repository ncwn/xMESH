#ifndef __CANARIN_LORAMSG__
#define __CANARIN_LORAMSG__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define AGGREGRATE_TOGETHER_MAX 32
#define BASE_TIMESTAMP 1609459200 // 1st Jan, 2021

enum can5_lmsg_sensor_type {
    LMSG_GLAT = 0,
    LMSG_GLNG,
    LMSG_GALT,
    LMSG_TEMP,
    LMSG_HUMI,
    LMSG_PRES,
    LMSG_PM2_5,
    LMSG_PM10,
    LMSG_PM1,
    LMSG_CO,
    LMSG_MQ7CO,
    LMSG_MHZ16CO2,
    LMSG_NO2,
    LMSG_VOLT,
    LMSG_WZSHCHO,
    LMSG_RAIN,
    LMSG_WIND_SPD,
    LMSG_WIND_DIR,

    // add new typed here

    LMSG_FORM,
    LMSG_STATUS,

    LMSG_ACK,
    LMSG_NR_ERROR,
    LMSG_NR_INTERVAL,
    LMSG_MAX,
};

#pragma pack(1)

struct can5_lmsg_data {
    uint8_t seq: 5;
    uint8_t cycle_id: 2;           /* basically avoid collision */
    uint8_t is_negative: 1;
    uint8_t num_bytes: 3;   /* number of bytes in data */
    uint8_t sensor_type: 5;
    uint8_t data[];
};

struct can5_lmsg_single_packet {
    union {
        struct {
            bool is_single: 1;
            uint32_t timestamp: 31;
        };
        uint32_t header;
    };
    uint8_t data[]; /* struct s_data array */
};

struct can5_lmsg_multi_packet {
    union {
        struct {
            bool is_single: 1;
            uint32_t timestamp: 31;
        };
        uint32_t header;
    };
    uint8_t n_data;
    uint8_t data[]; /* struct s_data array */
};

enum can5_lmsg_ack_type_e {
    LMSG_ACK_TYPE_DATA,
    LMSG_ACK_TYPE_RESTART,
    LMSG_ACK_TYPE_INTERVAL,
    LMSG_ACK_TYPE_RESET_FRAME_COUNTER,
    LMSG_ACK_TYPE_REJOIN,
};

struct can5_lmsg_ack {
    uint8_t cycle_id: 2;
    enum can5_lmsg_ack_type_e type: 6;
    uint32_t data;              /* we assume max 32 sequence(data points) per cycle */
};

struct can5_lmsg_packet {
    union {
        struct can5_lmsg_single_packet single;
        struct can5_lmsg_multi_packet multi;
        struct can5_lmsg_ack ack;
    };
};


#pragma pack()
/* datatype */
enum can5_lmsg_data_dtype {
    LMSG_DT_INT,
    LMSG_DT_FLOAT,
    LMSG_DT_STR,
    LMSG_DT_INVALID,
    LMSG_DT_MAX,
};

/* v5 */
size_t can5_lmsg_get_data_size(struct can5_lmsg_data *sd);

enum can5_lmsg_data_dtype can5_lmsg_get_datatype(enum can5_lmsg_sensor_type sensor_type);

int can5_lmsg_get_precision(enum can5_lmsg_sensor_type sensor_type);

struct can5_lmsg_data *can5_lmsg_make_sensor_data(enum can5_lmsg_sensor_type sensor_type,
                                                  uint8_t cycle_id, uint8_t seq, double data);

struct can5_lmsg_data *can5_lmsg_make_sensor_data_string(enum can5_lmsg_sensor_type sensor_type,
                                                         uint8_t cycle_id, uint8_t seq, char *data, size_t data_len);

void can5_lmsg_free_sensor_data(struct can5_lmsg_data *sd);

double can5_lmsg_parse_sensor_data(struct can5_lmsg_data *sd);

char *can5_lmsg_parse_sensor_string(struct can5_lmsg_data *sd);


struct can5_lmsg_packet *can5_lmsg_make_packet(uint64_t timestamp,
                                               struct can5_lmsg_data *sds[], uint8_t num_data, size_t *out_len);

struct can5_lmsg_packet *
can5_lmsg_make_ack_packet(enum can5_lmsg_ack_type_e type, uint32_t val, uint8_t nmembers, int id, int *seqs);

void can5_lmsg_free_packet(struct can5_lmsg_packet *packet);

struct can5_lmsg_packet *can5_lmsg_bin_to_packet(const char *data, int len);

void can5_lmsg_hton_packet(struct can5_lmsg_packet *packet);

int can5_lmsg_ntoh_packet(struct can5_lmsg_packet *packet, int len);

void can5_lmsg_hton_packet_ack(struct can5_lmsg_packet *packet);

int can5_lmsg_ntoh_packet_ack(struct can5_lmsg_packet *packet, int len);

bool can5_lmsg_is_single_packet(struct can5_lmsg_packet *packet);

uint8_t can5_lmsg_get_packet_n_data(struct can5_lmsg_packet *packet);

uint64_t can5_lmsg_get_packet_timestamp(struct can5_lmsg_packet *packet);

size_t can5_lmsg_get_packet_ack_length(struct can5_lmsg_packet *packet);

struct can5_lmsg_data *can5_lmsg_parse_packet(struct can5_lmsg_packet *packet, uint8_t index);

#endif
