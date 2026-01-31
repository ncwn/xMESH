/**************************************************
 * Author: rmukhia
 * Creation Date: 12/7/22
 * Description: 
 **************************************************/

#include <esp_log.h>
#include "can5_logstream_netsock.h"
#include "can5_log_stream.h"

static const char *TAG = "LOG_STREAM_NETSOCK";

static can5_err_t init(int index, const can5_logger_activate_params_t *params, active_cb active_cb);

static can5_err_t uninit();

static void logstream_log(const can5_logger_msg_t *msg);

static can5_err_t logstream_flush();

static bool is_active();

can5_logstream_t can5_logstream_netsock = {
    .type = CAN5_LOGGER_STREAM_NET_SOCKET,
    .init = init,
    .uninit = uninit,
    .flush = logstream_flush,
    .log = logstream_log,
    .is_active = is_active,
};

typedef struct tx_buf_s {
    char  buffer[CONFIG_CAN5_UDP_MTU];
    size_t len;
} tx_buf_t;

static struct {
    int net_sock;
    bool is_active;
    struct sockaddr_in dest_addr;
    tx_buf_t *tx;
} __log_stream = {
    .net_sock = -1,
    .is_active = false,
    .dest_addr = { 0 },
};

static can5_err_t __reset_socket();

static can5_err_t init(int index, const can5_logger_activate_params_t *params, active_cb active_cb)
{
    VERIFY_ALLOC(__log_stream.tx, sizeof(tx_buf_t));
    __log_stream.tx->len = 0;

    VERIFY_SUCCESS_SAFERETURN(__reset_socket(), {
        free(__log_stream.tx);
    });

    __log_stream.dest_addr.sin_addr.s_addr = inet_addr(params->net_socket.dest_ip);
    __log_stream.dest_addr.sin_port = htons(params->net_socket.port);
    __log_stream.dest_addr.sin_family = AF_INET;

    ESP_LOGI(TAG, "NetSock Logger [%s:%d]", params->net_socket.dest_ip, params->net_socket.port);

    __log_stream.is_active = true;
    active_cb(index);
    return CAN5_SUCCESS;
}

static can5_err_t uninit()
{
    // TODO: implement
    free(__log_stream.tx);
    return CAN5_SUCCESS;
}

#define UDP_HEADER_SIZE 8
static void logstream_log(const can5_logger_msg_t *msg)
{
    tx_buf_t *tx = __log_stream.tx;

    if (msg->len + tx->len < CONFIG_CAN5_UDP_MTU - UDP_HEADER_SIZE) {
        memcpy(tx->buffer + tx->len, msg->msg, msg->len);
        tx->len += msg->len;
    }
    else {
        if(sendto(__log_stream.net_sock, tx->buffer, tx->len, 0,
                  (struct sockaddr *) &__log_stream.dest_addr, sizeof(__log_stream.dest_addr)) < 0) {
            printf("Netsock Log: Error occurred during sending: errno %s\n", strerror(errno));

            __reset_socket();
        }
        memcpy(tx->buffer, msg->msg, msg->len);
        tx->len = msg->len;
    }
}

static can5_err_t logstream_flush()
{
    tx_buf_t *tx = __log_stream.tx;
    if (tx->len > 0) {
        if (sendto(__log_stream.net_sock, tx->buffer, tx->len, 0,
                   (struct sockaddr *) &__log_stream.dest_addr, sizeof(__log_stream.dest_addr)) < 0) {

            printf("Netsock Log: Error occurred during sending: errno %s\n", strerror(errno));

            __reset_socket();
        }
        tx->len = 0;
    }
    return CAN5_SUCCESS;
}

static bool is_active()
{
    return __log_stream.is_active;
}

static can5_err_t __reset_socket()
{
    if (__log_stream.net_sock >= 0) {
        closesocket(__log_stream.net_sock);
    }

    __log_stream.net_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (__log_stream.net_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return CAN5_NET_ERR_CONN_TIMEOUT;

    }

    return CAN5_SUCCESS;
}
