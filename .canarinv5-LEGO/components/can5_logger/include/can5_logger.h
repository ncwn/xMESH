/**************************************************
 * Author: rmukhia
 * Creation Date: 12/7/22
 * Description: 
 **************************************************/

#ifndef _CAN5_LOGGER_H_
#define _CAN5_LOGGER_H_

#include <lwip/sockets.h>
#include "can5_error.h"
#include "can5_module.h"

typedef enum can5_logger_stream_type_e {
    CAN5_LOGGER_STREAM_SD = 0x00,
    CAN5_LOGGER_STREAM_NET_SOCKET,

    CAN5_LOGGER_STREAM_COUNT,
} can5_logger_stream_type_t;

typedef struct can5_logger_activate_params_s {
    union {
        struct {
            const char *uri;
            int port;
            const char *username;
            const char *password;
            const char *client_id;
            const char *log_topic;
        } mqtt;

        struct {
            char dest_ip[IP4ADDR_STRLEN_MAX];
            int port;
        } net_socket;
    };

} can5_logger_activate_params_t;

typedef struct can5_logger_s {
    can5_module_t module;
    can5_err_t (*activate_stream)(can5_logger_stream_type_t stream, const can5_logger_activate_params_t *params);
    bool (*is_stream_active)(can5_logger_stream_type_t stream);
} can5_logger_t;


extern can5_logger_t can5_logger;

const char *can5_logger_stream_type_getstr(can5_logger_stream_type_t type);

#endif //_CAN5_LOGGER_H_
