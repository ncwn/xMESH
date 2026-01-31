/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */
/**
 * TODO:  bit fields is implementation defined. Use bit shift to properly make the packets portable.
 */

#include <malloc.h>
#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include <endian.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include "canarin_loramsg.h"

enum sensor_type get_sensor_type(char *ext)
{
	int i;
	enum sensor_type type;
	size_t size;

	type = ST_NR_ERROR;
	size = sizeof(sensor_type_ext)/sizeof(char *);

	for(i = 0; i < size; i++) {
		if(!strcmp(ext, sensor_type_ext[i])) {
			type = i;
			break;
		}
	}

	return type;
}

uint8_t get_bytes_needed(uint64_t data)
{
	if (data == 0)
		return 0;
	else if (data <= 0xFF)
		return 1;
	else if (data <= 0xFFFF)
		return 2;
	else if (data <= 0xFFFFFF)
		return 3;
	else if (data <= 0xFFFFFFFF)
		return 4;
	else if (data <= 0xFFFFFFFFFF)
		return 5;
	else if (data <= 0xFFFFFFFFFFFF)
		return 6;
	else if (data <= 0xFFFFFFFFFFFFFF)
		return 7;
	else
		return 8;
}


void write_data(uint64_t in, uint8_t *out, uint8_t num_bytes)
{
	while (num_bytes > 0) {
		out[--num_bytes] = in & 0xFF;
		in = in  >> 8;
	}
}

void write_data_float(float in, uint8_t *out)
{
    uint32_t * uin = (uint32_t  *)&in;
    size_t num_bytes = sizeof(float);

    assert(num_bytes == 4);
    // assume float in 32 bits in the target hw

    while (num_bytes > 0) {
        out[--num_bytes] = *uin & 0xFF;
        *uin = *uin  >> 8;
    }
}

void write_data_string(const char *in, uint8_t *out, uint8_t num_bytes)
{
    memcpy(out,in, num_bytes);
}


uint64_t read_data(const uint8_t *in, uint8_t num_bytes)
{
	uint64_t data, temp;
	uint8_t i =0;

	data = 0;
	while(num_bytes > 0) {
		temp = in[--num_bytes];
		data |= temp << (8 * i++);
	}
	return data;
}

float read_data_float(const uint8_t *in)
{
    uint32_t data, temp;
    size_t num_bytes = sizeof(float);

    assert(num_bytes == 4);
    uint8_t i =0;

    data = 0;
    while(num_bytes > 0) {
        temp = in[--num_bytes];
        data |= temp << (8 * i++);
    }
    return *(float *)&data;
}

struct s_data *make_sensor_data_int(enum sensor_type sensor_type,
                                    uint8_t seq, int64_t data)
{
	uint8_t num_bytes;
    bool is_negative;
	struct s_data *sd;

    is_negative = false;
    if (data < 0) {
        is_negative = true;
    }
    uint64_t udata = llabs(data);

	num_bytes = get_bytes_needed(udata);

	sd = calloc(1, sizeof(struct s_data) + num_bytes);
    sd->seq = seq;
    sd->is_negative = is_negative;
	sd->sensor_type = sensor_type;
	sd->num_bytes = num_bytes;
	write_data(udata, sd->data, num_bytes);

	return sd;
}

struct s_data *make_sensor_data_float(enum sensor_type sensor_type,
                                      uint8_t seq, float data)
{
    uint8_t num_bytes;
    bool is_negative;
    struct s_data *sd;

    is_negative = false;

    num_bytes = sizeof(float);

    sd = calloc(1, sizeof(struct s_data) + num_bytes);
    sd->seq = seq;
    sd->is_negative = is_negative;
    sd->sensor_type = sensor_type;
    sd->num_bytes = num_bytes;
    write_data_float(data, sd->data);

    return sd;
}

struct s_data *make_sensor_data_string(enum sensor_type sensor_type,
                                       uint8_t seq, char *data, size_t data_len)
{
    uint8_t num_bytes;
    bool is_negative;
    struct s_data *sd;

    is_negative = false;

    num_bytes = (uint8_t)data_len;

    sd = calloc(1, sizeof(struct s_data) + num_bytes);
    sd->seq = seq;
    sd->is_negative = is_negative;
    sd->sensor_type = sensor_type;
    sd->num_bytes = num_bytes;
    write_data_string(data, sd->data, num_bytes);

    return sd;
}

void free_sensor_data(struct s_data *sd)
{
	assert(sd);
	free(sd);
}

void parse_sensor_data(struct s_data *sd, enum s_data_dtype dtype, int64_t *l_data, float *f_data, char *s_data)
{
    int64_t data;
    switch (dtype) {
        case SD_INT:
            *l_data = read_data(sd->data, sd->num_bytes);
            if (sd->is_negative) {
                *l_data = *l_data * -1;
            }
            break;
        case SD_FLOAT:
            *f_data = read_data_float(sd->data);
            break;
        case SD_STR:
            memcpy(s_data, sd->data, sd->num_bytes);
            break;
        case SD_MAX:
            break;
    }
}

int64_t parse_sensor_data_int(struct s_data *sd)
{
    int64_t val;
    parse_sensor_data(sd, SD_INT, &val, NULL, NULL);
    return val;
}

float parse_sensor_data_float(struct s_data *sd)
{
    float val;
    parse_sensor_data(sd, SD_FLOAT, NULL, &val, NULL);
    return val;

}

char * parse_sensor_string(struct s_data *sd)
{
    char *val = malloc(sd->num_bytes);
    parse_sensor_data(sd, SD_STR, NULL, NULL, val);
    return val;
}

size_t get_s_data_size(struct s_data *sd)
{
	return (sizeof(struct s_data) + sd->num_bytes);
}


struct s_packet * make_packet(uint64_t timestamp,
		struct s_data *sds[], uint8_t num_data, size_t *out_len)
{
	struct s_packet *packet;
	uint8_t i;
	size_t total_data_size = 0;
	uint8_t *data;

	assert(num_data > 0);

	for (i = 0; i < num_data; i++)
		total_data_size += get_s_data_size(sds[i]);

	if (timestamp < BASE_TIMESTAMP)
		timestamp = BASE_TIMESTAMP;

	data = NULL;

	if (num_data == 1) {
		*out_len = sizeof(struct s_single_packet) + total_data_size;
		packet = calloc(1, *out_len);
		packet->single.is_single =  true;
		packet->single.timestamp = (timestamp - BASE_TIMESTAMP) & 0x8FFFFFFF;
		data = packet->single.data;
	}
	else {
		*out_len = sizeof(struct s_multi_packet) + total_data_size;
		packet = calloc(1, *out_len);
		packet->multi.is_single =  false;
		packet->multi.timestamp = (timestamp - BASE_TIMESTAMP) & 0x8FFFFFFF;
		packet->multi.n_data = num_data;
		data = packet->multi.data;
	}

	size_t index = 0;
	for (i = 0; i < num_data; i++) {
		size_t sd_size = get_s_data_size(sds[i]);
		memcpy(&data[index], sds[i], sd_size);
		index += sd_size;
	}

	return packet;
}


struct s_packet *can5_make_ack_packet(
		uint8_t nmembers, int *ids)
{
	struct s_packet *packet;
	uint8_t i;

	size_t len = sizeof(struct s_ack) + nmembers * sizeof(uint16_t);

	packet = calloc(len, 1);
	packet->ack.nmembers = nmembers;

	for(i = 0; i < nmembers; i++)
		packet->ack.ids[i] = ids[i];

	return packet;
}

void free_packet(struct s_packet *packet)
{
	// assert(packet);
	free(packet);
}

struct s_packet *can5_bin_to_s_packet(const char *data, int len)
{
	struct s_packet * pkt = malloc(len);
	memcpy(pkt, data, len);
	return pkt;
}

void can5_hton_s_packet(struct can5_s_packet *packet)
{
    uint8_t n,i;
    struct s_data *s_data;

    if (packet->single.is_single) {
        packet->single.header = htobe32(packet->single.header);
        s_data = (struct s_data*)&packet->single.data[0];
    }
    else {
        packet->multi.header = htobe32(packet->multi.header);
        n = packet->multi.n_data;
        for (i = 0; i < n; i++) {
            s_data = parse_packet(packet, i);
        }
    }
}

#define CHECK_SIZE()  if (len < expected_size) return false

int can5_ntoh_s_packet(struct can5_s_packet *packet, int len)
{
    uint8_t n,i;
    struct s_data *s_data;

    int expected_size = sizeof(uint32_t);

    // 1. decode header
    CHECK_SIZE();
    /* this will handle both multi and single packet header*/
    packet->single.header = be32toh(packet->single.header);

    if (is_single_packet(packet)) {
        // 2. decode single packet
        // 3. packet header + one s_data header
        expected_size = sizeof(struct s_single_packet) + sizeof(struct s_data);
        CHECK_SIZE();
        s_data = (struct s_data*)&packet->single.data[0];

        // 4. total size of packet
        expected_size += s_data->num_bytes;
        CHECK_SIZE();
    }
    else {
        // 2. decode multi packet
        expected_size = sizeof(struct s_multi_packet);
        CHECK_SIZE();
        // 3. get number of data
        n = packet->multi.n_data;
        for (i = 0; i < n; i++) {
            //4. atleast s_data header
            expected_size += sizeof(struct s_data);
            CHECK_SIZE();
            s_data = parse_packet(packet, i);
            // 5. add data size
            expected_size += s_data->num_bytes;
            CHECK_SIZE();
        }
    }
    // successfully parsed
    return true;
}

int can5_ntoh_s_packet_ack(struct can5_s_packet *packet, int len)
{
    int expected_size = sizeof(uint8_t) + (sizeof(uint16_t) * packet->ack.nmembers);
    CHECK_SIZE();

    for(uint8_t i = 0; i < packet->ack.nmembers; i++)
        packet->ack.ids[i] = be16toh(packet->ack.ids[i]);

    return true;
}
#undef CHECK_SIZE

void can5_hton_s_packet_ack(struct can5_s_packet *packet)
{
#if 0
    for(uint8_t i = 0; i < packet->ack.nmembers; i++)
        packet->ack.ids[i] = htobe16(packet->ack.ids[i]);
#endif
}

bool is_single_packet(struct s_packet *packet)
{
	return packet->single.is_single;
}

uint8_t get_packet_n_data(struct s_packet *packet)
{
	if (packet->single.is_single)
		return 1;
	else
		return packet->multi.n_data;
}

uint64_t get_packet_timestamp(struct s_packet *packet)
{
	uint64_t timestamp = BASE_TIMESTAMP;
	if (packet->single.is_single)
		timestamp += packet->single.timestamp;
	else
		timestamp += packet->multi.timestamp;
	return timestamp;
}

struct s_data * parse_packet(struct s_packet *packet, uint8_t index)
{
	size_t offset;
	bool is_single;
	uint8_t *data;

	assert(index >= 0);

	is_single = packet->single.is_single;

	if (is_single && index > 1)
		return NULL;

	if (!is_single && index >= packet->multi.n_data)
		return NULL;

	data = is_single ? packet->single.data : packet->multi.data;

	offset = 0;
	for (uint8_t i = 0; i < index; i ++) {

		offset +=  get_s_data_size((struct s_data *)&data[offset]);
	}

	return (struct s_data *)&data[offset];
}

size_t get_packet_ack_length(struct s_packet *packet)
{
	return sizeof(struct s_ack) + packet->ack.nmembers * sizeof(uint8_t);
}
