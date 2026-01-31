/**************************************************
 * Author: rmukhia
 * Creation Date: 12/7/22
 * Description: 
 **************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <freertos/portmacro.h>
#include "can5_logger.h"
#include "can5_utils.h"
#include "can5_storagemng.h"
#include "can5_netmng.h"
#include "can5_logstream_sd.h"
#include "can5_logstream_netsock.h"

static const char *TAG = "LOGGER";

static can5_err_t init();

static can5_err_t uninit();

static can5_err_t activate_stream(can5_logger_stream_type_t type, const can5_logger_activate_params_t *params);

can5_logger_t can5_logger = {
    .module = {
        .init = init,
        .uninit = uninit,
    },
    .activate_stream = activate_stream,
};

typedef enum logger_status_e {
    LOGGER_STATUS_UNINITD,
    LOGGER_STATUS_INITD,
    LOGGER_STATUS_ACTIVE,
} logger_status_t;

typedef struct log_buf_s {
    char buf[CONFIG_CAN5_LOGGER_QUEUE_SIZE][CONFIG_CAN5_LOGGER_MSG_BUFFER_SIZE];
    int index;
} log_buf_t;

typedef struct task_s {
    TaskHandle_t hdl;
    StaticTask_t buffer;
    StackType_t stack[CONFIG_CAN5_LOGGER_TASK_STACK_SIZE];
} task_t;

static struct {
    volatile logger_status_t status;
    QueueHandle_t msg_q;
    task_t task;
    can5_logstream_t *log_streams[CAN5_LOGGER_STREAM_COUNT];
    vprintf_like_t default_log_func;
    SemaphoreHandle_t log_mutex;
    log_buf_t *buf;
} __logger = {
    .status = LOGGER_STATUS_UNINITD,
    .msg_q = NULL,
    /* the following would be helpful to be in the order as
     * defined in can5_logstream_type_t
     */
    .log_streams = {
        &can5_logstream_sd,
        &can5_logstream_netsock,
    },
    .default_log_func = NULL,
    .log_mutex = NULL,
    .buf = NULL,
};

static int __log(const char *fmt, va_list list);

static void __logger_task(void *pv);

static can5_err_t init()
{
    if (__logger.status != LOGGER_STATUS_UNINITD) {
        return CAN5_ERR_INVALID_STATE;
    }


    __logger.log_mutex = xSemaphoreCreateMutex();
    VERIFY_NOT_NULL(__logger.log_mutex);

    __logger.msg_q = xQueueCreate(CONFIG_CAN5_LOGGER_QUEUE_SIZE, sizeof(can5_logger_msg_t));
    VERIFY_NOT_NULL(__logger.msg_q);

    VERIFY_ALLOC(__logger.buf, sizeof( log_buf_t));


    __logger.task.hdl = xTaskCreateStatic(__logger_task,
                                          "can5_logger_task",
                                          CONFIG_CAN5_LOGGER_TASK_STACK_SIZE,
                                          NULL,
                                          CONFIG_CAN5_LOGGER_TASK_PRIORITY,
                                          __logger.task.stack,
                                          &__logger.task.buffer);

    if (!__logger.task.hdl) {
        vQueueDelete(__logger.msg_q);
        vSemaphoreDelete(__logger.log_mutex);
        __logger.msg_q = NULL;
        return CAN5_ERR_FREERTOS_pdFAIL;
    }


    __logger.status = LOGGER_STATUS_INITD;
    return CAN5_SUCCESS;
}

static can5_err_t uninit()
{
    if (__logger.status == LOGGER_STATUS_UNINITD) {
        return CAN5_ERR_INVALID_STATE;
    }

    for (size_t i = 0; i < CAN5_LOGGER_STREAM_COUNT; i++) {
        if (__logger.log_streams[i]->is_active()) {
            __logger.log_streams[i]->flush();
            __logger.log_streams[i]->uninit();
        }
    }

    // TODO: remove queue and task
    free(__logger.buf);
    __logger.status = LOGGER_STATUS_UNINITD;

    return CAN5_SUCCESS;
}

static void __active_cb(int index)
{
    ESP_LOGI(TAG, "Log Stream %s Active.", can5_logger_stream_type_getstr(__logger.log_streams[index]->type));
    if (__logger.status != LOGGER_STATUS_ACTIVE) {
        // Enable the esp log hook
        __logger.default_log_func = esp_log_set_vprintf(__log);

        __logger.status = LOGGER_STATUS_ACTIVE;

    }
}

static can5_err_t activate_stream(can5_logger_stream_type_t type, const can5_logger_activate_params_t *params)
{
    if (__logger.status == LOGGER_STATUS_UNINITD) {
        return CAN5_ERR_INVALID_STATE;
    }

    for (size_t i = 0; i < CAN5_LOGGER_STREAM_COUNT; i++) {
        if (__logger.log_streams[i]->type == type) {
            if (__logger.log_streams[i]->is_active()) {
                return CAN5_SUCCESS;
            }

            ESP_LOGI(TAG,"Activating %s", can5_logger_stream_type_getstr(type));
            VERIFY_SUCCESS(__logger.log_streams[i]->init(i, params, __active_cb));

            return CAN5_SUCCESS;

        }
    }

    return CAN5_ERR_INVALID_PARAM;
}

static BaseType_t __add_to_queue(const can5_logger_msg_t *msg)
{
    return xQueueSend(__logger.msg_q, msg, pdMS_TO_TICKS(50));
}


static int __log(const char *fmt, va_list list)
{
    int ret = 0;
    can5_logger_msg_t msg;
    log_buf_t *buf = __logger.buf;

    if (__logger.default_log_func) {
        ret = __logger.default_log_func(fmt, list);
    }

    msg.len = vsnprintf(NULL, 0, fmt, list);

    if (msg.len > 0) {

        //msg.msg = malloc(msg.len + 1);
        if (xSemaphoreTake(__logger.log_mutex, portMAX_DELAY) == pdTRUE) {
            buf->index = (buf->index + 1) % CONFIG_CAN5_LOGGER_QUEUE_SIZE;
            xSemaphoreGive(__logger.log_mutex);
        }
        msg.msg = (char *) buf->buf[buf->index];
        vsnprintf(msg.msg, msg.len + 1, fmt, list);

        for (size_t i = 0; i < CAN5_LOGGER_STREAM_COUNT; i++) {

            if (__logger.log_streams[i]->is_active()) {
                can5_logger_msg_t lmsg = msg;
                lmsg.log_cb = __logger.log_streams[i]->log;

                __add_to_queue(&lmsg);
            }
        }

        // add a final message to free the allocated memory
        //msg.log_cb = __free_msg;

        // make sure free is executed
        //while (__add_to_queue(&msg, portMAX_DELAY) == pdFALSE) {
        //vTaskDelay(pdMS_TO_TICKS(100));
        //}
    }

    return ret;
}

static can5_logger_msg_t msg;
static void __logger_task(void *pv)
{
    for (;;) {
        if (xQueueReceive(__logger.msg_q, &msg, portMAX_DELAY) != pdTRUE) {
            // did not receive message
            continue;
        }

        // execute logging functions
        msg.log_cb(&msg);
    }
}

static const can5_tag_tab_t _logger_stream_type_tags = {
    TAG_TAB_ITEM(CAN5_LOGGER_STREAM_SD),
    TAG_TAB_ITEM(CAN5_LOGGER_STREAM_NET_SOCKET),
};

const char *can5_logger_stream_type_getstr(can5_logger_stream_type_t type)
{
    return TAG_LOOKUP(type, _logger_stream_type_tags);
}
