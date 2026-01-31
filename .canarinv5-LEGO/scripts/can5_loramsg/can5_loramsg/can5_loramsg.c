/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */
/**
 * TODO:  bit fields is implementation defined. Use bit shift to properly make the packets portable.
 */

#include <malloc.h>
#include <alloca.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <endian.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include "can5_loramsg.h"


#define DEF_PRECISION(typ, dtyp, dec)         {.type = typ, .dtype= dtyp, .decimal_places= dec }


/* define the precision of integer data types */
static struct {
    enum can5_lmsg_sensor_type type;
    enum can5_lmsg_data_dtype dtype;
    uint8_t decimal_places;
} __can5_lmsg_meta[] = {

    // integer types
    DEF_PRECISION(LMSG_GALT, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_TEMP, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_HUMI, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_PRES, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_PM2_5, LMSG_DT_INT, 0),
    DEF_PRECISION(LMSG_PM10, LMSG_DT_INT, 0),
    DEF_PRECISION(LMSG_PM1, LMSG_DT_INT, 0),
    DEF_PRECISION(LMSG_CO, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_MQ7CO, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_MHZ16CO2, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_NO2, LMSG_DT_INT, 0),
    DEF_PRECISION(LMSG_VOLT, LMSG_DT_INT, 1),
    DEF_PRECISION(LMSG_RAIN, LMSG_DT_INT, 2),
    DEF_PRECISION(LMSG_WIND_SPD, LMSG_DT_INT, 2),
    DEF_PRECISION(LMSG_WIND_DIR, LMSG_DT_INT, 2),

    // float types, precision is ignored
    DEF_PRECISION(LMSG_GLAT, LMSG_DT_FLOAT, 7),
    DEF_PRECISION(LMSG_GLNG, LMSG_DT_FLOAT, 7),

    // ignored types
    DEF_PRECISION(LMSG_NR_ERROR, LMSG_DT_INVALID, 0),
    DEF_PRECISION(LMSG_NR_INTERVAL, LMSG_DT_INVALID, 0),
    DEF_PRECISION(LMSG_STATUS, LMSG_DT_INVALID, 0),
    DEF_PRECISION(LMSG_ACK, LMSG_DT_INVALID, 0),
    DEF_PRECISION(LMSG_WZSHCHO, LMSG_DT_INVALID, 1),
    DEF_PRECISION(LMSG_FORM, LMSG_DT_INVALID, 0),
};

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

struct can5_lmsg_data *can5_lmsg_make_sensor_data(enum can5_lmsg_sensor_type sensor_type,
                                                  uint8_t cycle_id, uint8_t seq, double data)
{
    uint8_t num_bytes;
    bool is_negative;
    struct can5_lmsg_data *sd;
    enum can5_lmsg_data_dtype data_type;

    data_type = can5_lmsg_get_datatype(sensor_type);

    if (data_type == LMSG_DT_INT) {
        double val;
        uint64_t udata;
        int precision;

        is_negative = false;
        if (data < 0) {
            is_negative = true;
        }

        precision = can5_lmsg_get_precision(sensor_type);

        val = data;
        if (precision) {
            val *= (10 *precision);
        }

        udata = (uint64_t)fabs(val);

        num_bytes = get_bytes_needed(udata);

        sd = calloc(1, sizeof(struct can5_lmsg_data) + num_bytes);
        write_data(udata, sd->data, num_bytes);

    }
    else if (data_type == LMSG_DT_FLOAT) {
        is_negative = false;
        num_bytes = sizeof(float);

        sd = calloc(1, sizeof(struct can5_lmsg_data) + num_bytes);
        write_data_float(data, sd->data);

    }
    else {
        return NULL;
    }

    sd->cycle_id = cycle_id;
    sd->seq = seq;
    sd->is_negative = is_negative;
    sd->sensor_type = sensor_type;
    sd->num_bytes = num_bytes;

    return sd;
}

struct can5_lmsg_data * can5_lmsg_make_sensor_data_string(enum can5_lmsg_sensor_type sensor_type,
                                                          uint8_t cycle_id, uint8_t seq, char *data, size_t data_len)
{
    uint8_t num_bytes;
    bool is_negative;
    struct can5_lmsg_data *sd;

    is_negative = false;

    num_bytes = (uint8_t)data_len;

    sd = calloc(1, sizeof(struct can5_lmsg_data) + num_bytes);
    sd->cycle_id = cycle_id;
    sd->seq = seq;
    sd->is_negative = is_negative;
    sd->sensor_type = sensor_type;
    sd->num_bytes = num_bytes;
    write_data_string(data, sd->data, num_bytes);

    return sd;
}

void can5_lmsg_free_sensor_data(struct can5_lmsg_data *sd)
{
	assert(sd);
	free(sd);
}

enum can5_lmsg_data_dtype can5_lmsg_get_datatype(enum can5_lmsg_sensor_type sensor_type)
{
    for(size_t i = 0; i < sizeof(__can5_lmsg_meta) / sizeof(__can5_lmsg_meta[0]); i++){
        if (sensor_type == __can5_lmsg_meta[i].type) {
            return __can5_lmsg_meta[i].dtype;
        }
    }
    return LMSG_DT_INVALID;
}

int can5_lmsg_get_precision(enum can5_lmsg_sensor_type sensor_type)
{
    for(size_t i = 0; i < sizeof(__can5_lmsg_meta) / sizeof(__can5_lmsg_meta[0]); i++){
        if (sensor_type == __can5_lmsg_meta[i].type) {
            return __can5_lmsg_meta[i].decimal_places;
        }
    }
    return 0;
}

double can5_lmsg_parse_sensor_data(struct can5_lmsg_data *sd)
{
    double result;
    int64_t i_val;
    float f_val;
    int precision;
    enum can5_lmsg_data_dtype data_type;

    data_type = can5_lmsg_get_datatype(sd->sensor_type);
    precision = can5_lmsg_get_precision(sd->sensor_type);

    switch (data_type) {
        case LMSG_DT_INT:
            i_val = (int64_t)read_data(sd->data, sd->num_bytes);
            if (sd->is_negative) {
                i_val = i_val * -1;
            }

            result = (double) i_val;

            if (precision) {
                result /= (10 * precision);
            }

            break;

        case LMSG_DT_FLOAT:
            f_val = read_data_float(sd->data);
            result = f_val;
            break;

        case LMSG_DT_STR:
        case LMSG_DT_MAX:
        case LMSG_DT_INVALID:
        default:
            result = 0;
            break;
    }

    return result;
}


char * can5_lmsg_parse_sensor_string(struct can5_lmsg_data *sd)
{
    char *val;
    enum can5_lmsg_data_dtype data_type;
    data_type = can5_lmsg_get_datatype(sd->sensor_type);

    if (data_type != LMSG_DT_STR) {
        return NULL;
    }

    val = malloc(sd->num_bytes);
    memcpy(val, sd->data, sd->num_bytes);
    return val;
}

size_t can5_lmsg_get_data_size(struct can5_lmsg_data *sd)
{
	return (sizeof(struct can5_lmsg_data) + sd->num_bytes);
}


struct can5_lmsg_packet * can5_lmsg_make_packet(uint64_t timestamp,
                                                struct can5_lmsg_data *sds[], uint8_t num_data, size_t *out_len)
{
	struct can5_lmsg_packet *packet;
	uint8_t i;
	size_t total_data_size = 0;
	uint8_t *data;

	assert(num_data > 0);

	for (i = 0; i < num_data; i++)
		total_data_size += can5_lmsg_get_data_size(sds[i]);

	if (timestamp < BASE_TIMESTAMP)
		timestamp = BASE_TIMESTAMP;

	data = NULL;

	if (num_data == 1) {
		*out_len = sizeof(struct can5_lmsg_single_packet) + total_data_size;
		packet = calloc(1, *out_len);
		packet->single.is_single =  true;
		packet->single.timestamp = (timestamp - BASE_TIMESTAMP) & 0x8FFFFFFF;
		data = packet->single.data;
	}
	else {
		*out_len = sizeof(struct can5_lmsg_multi_packet) + total_data_size;
		packet = calloc(1, *out_len);
		packet->multi.is_single =  false;
		packet->multi.timestamp = (timestamp - BASE_TIMESTAMP) & 0x8FFFFFFF;
		packet->multi.n_data = num_data;
		data = packet->multi.data;
	}

	size_t index = 0;
	for (i = 0; i < num_data; i++) {
		size_t sd_size = can5_lmsg_get_data_size(sds[i]);
		memcpy(&data[index], sds[i], sd_size);
		index += sd_size;
	}

	return packet;
}


struct can5_lmsg_packet *can5_lmsg_make_ack_packet(enum can5_lmsg_ack_type_e type, uint32_t val, uint8_t nmembers, int id, int *seqs)
{
	struct can5_lmsg_packet *packet;
	uint8_t i;
    size_t len = sizeof(struct can5_lmsg_ack);

    packet = calloc(len, 1);
    packet->ack.type = type;
    switch (type) {

        case LMSG_ACK_TYPE_DATA:
            packet->ack.cycle_id = id;

            packet->ack.data = 0;

            for(i = 0; i < nmembers; i++) {
                packet->ack.data |= (1 << seqs[i]);
            }

            break;
        case LMSG_ACK_TYPE_RESTART:
            break;
        case LMSG_ACK_TYPE_INTERVAL:
            packet->ack.data = val;
            break;
        case LMSG_ACK_TYPE_RESET_FRAME_COUNTER:
        case LMSG_ACK_TYPE_REJOIN:
        default:
            break;
    }

	return packet;
}

void can5_lmsg_free_packet(struct can5_lmsg_packet *packet)
{
	// assert(packet);
	free(packet);
}

struct can5_lmsg_packet *can5_lmsg_bin_to_packet(const char *data, int len)
{
	struct can5_lmsg_packet * pkt = malloc(len);
	memcpy(pkt, data, len);
	return pkt;
}

void can5_lmsg_hton_packet(struct can5_lmsg_packet *packet)
{
    //uint8_t n,i;
    // struct can5_s_data *s_data;

    if (packet->single.is_single) {
        packet->single.header = htobe32(packet->single.header);
        //s_data = (struct can5_s_data*)&packet->single.data[0];
    }
    else {
        packet->multi.header = htobe32(packet->multi.header);
        //n = packet->multi.n_data;
        //for (i = 0; i < n; i++) {
            //s_data = can5_parse_packet(packet, i);
        //}
    }
}

#define CHECK_SIZE()  if (len < expected_size) return false

int can5_lmsg_ntoh_packet(struct can5_lmsg_packet *packet, int len)
{
    uint8_t n,i;
    struct can5_lmsg_data *s_data;

    int expected_size = sizeof(uint32_t);

    // 1. decode header
    CHECK_SIZE();
    /* this will handle both multi and single packet header*/
    packet->single.header = be32toh(packet->single.header);

    if (can5_lmsg_is_single_packet(packet)) {
        // 2. decode single packet
        // 3. packet header + one s_data header
        expected_size = sizeof(struct can5_lmsg_single_packet) + sizeof(struct can5_lmsg_data);
        CHECK_SIZE();
        s_data = (struct can5_lmsg_data*)&packet->single.data[0];

        // 4. total size of packet
        expected_size += s_data->num_bytes;
        CHECK_SIZE();
    }
    else {
        // 2. decode multi packet
        expected_size = sizeof(struct can5_lmsg_multi_packet);
        CHECK_SIZE();
        // 3. get number of data
        n = packet->multi.n_data;
        for (i = 0; i < n; i++) {
            //4. atleast s_data header
            expected_size += sizeof(struct can5_lmsg_data);
            CHECK_SIZE();
            s_data = can5_lmsg_parse_packet(packet, i);
            // 5. add data size
            expected_size += s_data->num_bytes;
            CHECK_SIZE();
        }
    }
    // successfully parsed
    return true;
}

int can5_lmsg_ntoh_packet_ack(struct can5_lmsg_packet *packet, int len)
{
    int expected_size = sizeof(struct can5_lmsg_ack);
    CHECK_SIZE();

    packet->ack.data = be32toh(packet->ack.data);
    return true;
}

#undef CHECK_SIZE

void can5_lmsg_hton_packet_ack(struct can5_lmsg_packet *packet)
{
    packet->ack.data = htobe32(packet->ack.data);
}

bool can5_lmsg_is_single_packet(struct can5_lmsg_packet *packet)
{
	return packet->single.is_single;
}

uint8_t can5_lmsg_get_packet_n_data(struct can5_lmsg_packet *packet)
{
	if (packet->single.is_single)
		return 1;
	else
		return packet->multi.n_data;
}

uint64_t can5_lmsg_get_packet_timestamp(struct can5_lmsg_packet *packet)
{
	uint64_t timestamp = BASE_TIMESTAMP;
	if (packet->single.is_single)
		timestamp += packet->single.timestamp;
	else
		timestamp += packet->multi.timestamp;
	return timestamp;
}

struct can5_lmsg_data * can5_lmsg_parse_packet(struct can5_lmsg_packet *packet, uint8_t index)
{
	size_t offset;
	bool is_single;
	uint8_t *data;

	is_single = packet->single.is_single;

	if (is_single && index > 1)
		return NULL;

	if (!is_single && index >= packet->multi.n_data)
		return NULL;

	data = is_single ? packet->single.data : packet->multi.data;

	offset = 0;
	for (uint8_t i = 0; i < index; i ++) {

		offset += can5_lmsg_get_data_size((struct can5_lmsg_data *) &data[offset]);
	}

	return (struct can5_lmsg_data *)&data[offset];
}

size_t can5_lmsg_get_packet_ack_length(struct can5_lmsg_packet *packet)
{
	return sizeof(struct can5_lmsg_ack);
}
