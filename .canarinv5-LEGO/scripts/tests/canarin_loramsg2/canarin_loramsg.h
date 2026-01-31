/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __CANARIN_LORAMSG__
#define __CANARIN_LORAMSG__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#define AGGREGRATE_TOGETHER_MAX 10
#define BASE_TIMESTAMP 1609459200 // 1st Jan, 2021

/* sensor data types */
/* ST_.. : Sensor Type: Actual physical sensor data
 * ST_NR_.. : Sensor Type Not Real: Not real sensor data, but config types
 * !!!!IMPORTANT!!!
 * Index of enum sensor_type and statuc char *sensor_type_ext[] should match.
 */

enum sensor_type {
	ST_NR_ERROR,
	ST_NR_INTERVAL,
	ST_GLAT,
	ST_GLNG,
	ST_GALT,
	ST_TEMP,
	ST_HUMI,
	ST_PRES,
	ST_PM2_5,
	ST_PM10,
	ST_PM1,
	ST_VOLT,
	ST_FORM,
	ST_STATUS,
	ST_MQ7CO,
	ST_WZSHCHO,
	ST_MHZ16CO2,
	ST_INIT_PKT,
	ST_GLAT_INT,
	ST_GLAT_FRAC,
	ST_GLNG_INT,
	ST_GLNG_FRAC,
	ST_GALT_INT,
	ST_ACK,
	ST_MAX,
};


static char *sensor_type_ext[24] = {
	"error",
	"interval",
	"glat",
	"glng",
	"galt",
	"temp",
	"humi",
	"pres",
	"pm25",
	"pm10",
	"pm1",
	"volt",
	"form",
	"status",
	"co",
	"wzshcho",
	"mhz16co2",
};

#pragma pack(1)

struct s_data {
    uint8_t seq:7;
    uint8_t is_negative:1;
	uint8_t num_bytes: 3; /* number of bytes in data */
	uint8_t sensor_type :5;
	uint8_t data[];
};

struct s_single_packet {
    union {
        struct {
			bool is_single: 1;
            uint32_t timestamp: 31;
		};
		uint32_t header;
	};
	uint8_t data[]; /* struct s_data array */
};

struct s_multi_packet {
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

struct s_ack {
	uint8_t nmembers;
	uint8_t ids[];
};

struct s_packet {
	union {
		struct s_single_packet single;
		struct s_multi_packet multi;
		struct s_ack ack;
	};
};


#pragma pack()
/* datatype */
enum s_data_dtype {
    SD_INT,
    SD_FLOAT,
    SD_STR,
    SD_MAX,
};

enum sensor_type get_sensor_type(char *ext);
/* v4.1 */
size_t get_s_data_size(struct s_data *sd);

struct s_data *make_sensor_data_int(enum sensor_type sensor_type,
                                    uint8_t seq, int64_t data);

struct s_data *make_sensor_data_float(enum sensor_type sensor_type,
                                      uint8_t seq, float data);

struct s_data *make_sensor_data_string(enum sensor_type sensor_type,
                                       uint8_t seq, char *data, size_t data_len);

void free_sensor_data(struct s_data *sd);

int64_t parse_sensor_data_int(struct s_data *sd);
float parse_sensor_data_float(struct s_data *sd);
char * parse_sensor_string(struct s_data *sd);

struct s_packet *make_packet(uint64_t timestamp,
		struct s_data *sds[], uint8_t num_data, size_t *out_len);

struct s_packet *can5_make_ack_packet(uint8_t nmembers, int *ids);

void free_packet(struct s_packet *packet);

struct s_packet *can5_bin_to_s_packet(const char *data, int len);

void can5_hton_s_packet(struct can5_s_packet *packet);

int can5_ntoh_s_packet(struct can5_s_packet *packet, int len);

void can5_hton_s_packet_ack(struct can5_s_packet *packet);

int can5_ntoh_s_packet_ack(struct can5_s_packet *packet, int len);

bool is_single_packet(struct s_packet *packet);

uint8_t get_packet_n_data(struct s_packet *packet);

uint64_t get_packet_timestamp(struct s_packet *packet);

size_t get_packet_ack_length(struct s_packet *packet);

struct s_data * parse_packet(struct s_packet *packet, uint8_t index);

#endif
