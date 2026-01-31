/**************************************************
 * Author: rmukhia
 * Creation Date: 30/9/22
 * Description: 
 **************************************************/

#include <esp_log.h>
#include "can5_actuator_relay.h"
#include "can5_utils.h"

static const char *TAG = "ACTUATOR_RELAY";

#if 1
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif

static can5_actuator_hdl_t *alloc(size_t *len);
static uint8_t get_id(can5_actuator_hdl_t *hdl);
static void set_id(can5_actuator_hdl_t *hdl, uint8_t id);
static can5_err_t detect(can5_actuator_hdl_t *hdl, can5_port_idx_t port);
static can5_err_t init(can5_actuator_hdl_t *hdl, can5_port_idx_t port);
static can5_err_t uninit(can5_actuator_hdl_t *hdl);
static can5_err_t command(can5_actuator_hdl_t *hdl,
                          can5_actuator_cmd_t cmd, const can5_actuator_cmd_params_t *params);


const can5_actuatordriv_t actuatordriv_relay = {
    .ops = {
        .alloc = alloc,
        .get_id = get_id,
        .set_id = set_id,
        .detect = detect,
        .init = init,
        .uninit = uninit,
        .command = command,
    },
    .details = {
        .actuator = {
            .type = CAN5_ACTUATORDRIV_TYPE_RELAY,
        },
        .io_type = CAN5_PHY_IO_TYPE_ADC,
        .name = "Relay",
        .version = "1.0",
        .manufacturer = "interlab",
    }
};

typedef struct actuator_hdl_s {
    uint8_t id;
    can5_port_idx_t port;
} actuator_hdl_t;


static can5_actuator_hdl_t *alloc(size_t *len)
{
    can5_actuator_hdl_t *hdl;
    actuator_hdl_t *a_hdl;
    *len = sizeof(can5_actuator_hdl_t) + sizeof(actuator_hdl_t);

    hdl = malloc(*len);
    if (!hdl) {
        return NULL;
    }

    a_hdl = hdl->hdl = hdl->hdl_data;

    CLEAR_STRUCT(*a_hdl);
    a_hdl->port = CAN5_PORT_NULL;
    a_hdl->id = CAN5_ACTUATOR_ID_NONE;

    return hdl;
}

static uint8_t get_id(can5_actuator_hdl_t *hdl)
{
    TRACE_FUNC;
    actuator_hdl_t  *a_hdl = hdl->hdl;

    return a_hdl->id;
}

static void set_id(can5_actuator_hdl_t *hdl, uint8_t id)
{
    TRACE_FUNC;
    actuator_hdl_t *a_hdl = hdl->hdl;

    a_hdl->id = id;
}

static can5_err_t detect(can5_actuator_hdl_t *hdl, can5_port_idx_t port)
{
    TRACE_FUNC;
    return CAN5_SUCCESS;
}

static can5_err_t init(can5_actuator_hdl_t *hdl, can5_port_idx_t port)
{
    TRACE_FUNC;
    return CAN5_SUCCESS;
}

static can5_err_t uninit(can5_actuator_hdl_t *hdl)
{
    TRACE_FUNC;
    return CAN5_SUCCESS;
}

static can5_err_t command(can5_actuator_hdl_t *hdl,
                          can5_actuator_cmd_t cmd, const can5_actuator_cmd_params_t *params)
{
    TRACE_FUNC;
    return CAN5_SUCCESS;
}
