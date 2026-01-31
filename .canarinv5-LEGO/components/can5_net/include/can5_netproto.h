/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   09/02/22
*
* File:  can5_netstrat.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_NETSTRAT_H
#define CAN5_APP_CAN5_NETSTRAT_H
#include "can5_netif.h"

/**
 * @brief Event type propogated by netproto to netmng.
 */
typedef enum can5_netproto_evt_type_e {
    CAN5_NETPROTO_EVT_NONE = 0,
    CAN5_NETPROTO_EVT_COMPLETE,
    CAN5_NETPROTO_EVT_FAILED,

    CAN5_NETPROTO_EVT_SEND_ATTEMPTS_OVER,

    CAN5_NETPROTO_EVT_COUNT,
} can5_netproto_evt_type_t;

/**
 * @brief Event propagated by netproto to network manager.
 */
typedef struct can5_netproto_evt_s {
    uint8_t netproto_id;
    can5_netproto_evt_type_t type;
} can5_netproto_evt_t;


/**
 * @brief Callback definition for netproto events.
 */
typedef void (can5_netproto_send_cb_f)(const can5_netproto_evt_t *evt);

/**
 * @brief strategy to use for net communication.
 */
typedef struct can5_netproto_s {
    can5_netproto_type_t type;

    uint8_t (*get_id)();

    void (*set_id)(uint8_t id);

    /**
     * @brief Register the strategy with a net interface driver.
     *
     * @param netif  Netif driver to register strategy with.
     * @return CAN5 error type.
     */
    can5_err_t (*init)(const can5_netif_t *netif);

    /**
     * @brief Start the strategy cycle.
     *
     * @return CAN5 error type.
     */
    can5_err_t (*start)();

    /**
     * @brief Send data through the strategy.
     *
     * @return CAN5 error type.
     */
    can5_err_t (*send)(const uint8_t *data, size_t len, can5_netproto_send_cb_f send_cb);
    /**
     * @brief End/Abort the strategy cycle.
     * @return CAN5 error type.
     */
    can5_err_t (*end)();

    /**
     * @brief Run method called from main loop.
     * @return CAN5 errror type.
     */
    can5_err_t (*run)();

    /**
     * @brief Intercept the net connect evt from the netif to netmng.
     * @param evt
     */
    void (*forward_netif_connevt)(const can5_net_connect_evt_t *evt);

    /**
     * @brief Intercept the rx from netif.
     * @param data
     * @param len
     */
    void (*forward_netif_rx)(const void *data, size_t len);
} can5_netproto_t;

extern const can5_netproto_t netproto_hazemon;
extern const can5_netproto_t netproto_lwan;
extern const can5_netproto_t netproto_mqtt;
#endif //CAN5_APP_CAN5_NETSTRAT_H
