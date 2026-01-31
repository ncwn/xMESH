
/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   04/02/22
*
* File:  can5_types.c
* Descr:
*******************************************************************************/
#include "can5_types.h"
#include "can5_utils.h"

static const can5_tag_tab_t _sensordriv_type_tags = {
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_PM),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_CO2),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_GPS),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_CO_ADC),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_CO),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_BME680),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_BME280),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_NO2),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_SIM7600_GPS),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_WS3226),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_MPU6500),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_SCD41),
    TAG_TAB_ITEM(CAN5_SENSORDRIV_TYPE_NONE),
};

const char *can5_sensor_type_getstr(int type)
{
    return TAG_LOOKUP(type, _sensordriv_type_tags);
}

static const can5_tag_tab_t _actuatordriv_type_tags = {
    TAG_TAB_ITEM(CAN5_ACTUATORDRIV_TYPE_RELAY),
    TAG_TAB_ITEM(CAN5_ACTUATORDRIV_TYPE_NONE),
};

const char *can5_actuator_type_getstr(can5_actuatordriv_type_t type)
{
    return TAG_LOOKUP(type, _actuatordriv_type_tags);
}

static const can5_tag_tab_t _storage_type_tags = {
    TAG_TAB_ITEM(CAN5_STORAGE_TYPE_SDSPI),
    TAG_TAB_ITEM(CAN5_STORAGE_TYPE_RAM),
    TAG_TAB_ITEM(CAN5_STORAGE_TYPE_NVS),
    TAG_TAB_ITEM(CAN5_STORAGE_TYPE_NONE),
};

const char *can5_storagedriv_type_getstr(can5_storagedriv_type_t type)
{
    return TAG_LOOKUP(type, _storage_type_tags);
}

/**
 * @brief Hal status tags
 *
 */
static const can5_tag_tab_t _can5_port_tags = {
    TAG_TAB_ITEM(NETPORT_0),
    TAG_TAB_ITEM(NETPORT_1),
    TAG_TAB_ITEM(ADPORT_0),
    TAG_TAB_ITEM(ADPORT_1),
    TAG_TAB_ITEM(ADPORT_2),
    TAG_TAB_ITEM(ADPORT_3),
    TAG_TAB_ITEM(UPORT_0),
    TAG_TAB_ITEM(UPORT_1),
    TAG_TAB_ITEM(UPORT_2),
    TAG_TAB_ITEM(UPORT_3),
    TAG_TAB_ITEM(UPORT_4),
    TAG_TAB_ITEM(UPORT_5),
    TAG_TAB_ITEM(UPORT_6),
    TAG_TAB_ITEM(UPORT_7),
    TAG_TAB_ITEM(I2C_0),
    TAG_TAB_ITEM(CAN5_PORT_NULL),
};

const char *can5_hal_port_getstr(int32_t evt)
{
    return TAG_LOOKUP(evt, _can5_port_tags);
}

/**
 * @brief Netif types tags
 *
 */
static const can5_tag_tab_t _net_type_tags = {
    TAG_TAB_ITEM(CAN5_NET_TYPE_LWIP),
    TAG_TAB_ITEM(CAN5_NET_TYPE_LORAWAN),
    TAG_TAB_ITEM(CAN5_NET_TYPE_NONE)
};

const char *can5_netif_getstr(can5_netif_type_t type)
{
    return TAG_LOOKUP(type, _net_type_tags);
}

/**
 * @brief Device mode tags
 *
 */

static const can5_tag_tab_t _device_mode_tags = {
    TAG_TAB_ITEM(CAN5_MODE_NORMAL),
    TAG_TAB_ITEM(CAN5_MODE_CONFIG),
};


const char *can5_device_mode_getstr(can5_device_mode_t mode)
{
    return TAG_LOOKUP(mode, _device_mode_tags);
}


static const can5_tag_tab_t _netproto_type_tags = {
    TAG_TAB_ITEM(CAN5_NETPROTO_HAZEMON),
    TAG_TAB_ITEM(CAN5_NETPROTO_LORARELAY),
    TAG_TAB_ITEM(CAN5_NETPROTO_MQTT),
    TAG_TAB_ITEM(CAN5_NETPROTO_COUNT),
};

const char *can5_netproto_type_get_str(can5_netproto_type_t type)
{
    return TAG_LOOKUP(type, _netproto_type_tags);
}

static const can5_tag_tab_t  _device_data_mode_tags = {
    TAG_TAB_ITEM(CAN5_DATA_MODE_CYCLE),
    TAG_TAB_ITEM(CAN5_DATA_MODE_REALTIME),
};

const char* can5_device_data_mode_get_str(can5_device_data_mode_t type)
{
    return TAG_LOOKUP(type, _device_data_mode_tags);
}
