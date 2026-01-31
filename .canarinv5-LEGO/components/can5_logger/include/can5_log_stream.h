/**************************************************
 * Author: rmukhia
 * Creation Date: 12/7/22
 * Description: 
 **************************************************/

#ifndef CANARINV5_LEGO_CAN5_LOG_STREAM_H
#define CANARINV5_LEGO_CAN5_LOG_STREAM_H

#include "can5_logger.h"

typedef void(*active_cb)(int index);

typedef struct can5_logger_msg_s {
    can5_logger_stream_type_t type;
    char *msg;
    size_t len;

    void (*log_cb)(const struct can5_logger_msg_s *msg);
} can5_logger_msg_t;

typedef struct can5_logstream_s {
    can5_logger_stream_type_t type;

    can5_err_t (*init)(int index, const can5_logger_activate_params_t *params, active_cb cb);

    can5_err_t (*uninit)();

    can5_err_t (*flush)();

    bool (*is_active)();

    void (*log)(const can5_logger_msg_t *msg);

} can5_logstream_t;

#endif //CANARINV5_LEGO_CAN5_LOG_STREAM_H
